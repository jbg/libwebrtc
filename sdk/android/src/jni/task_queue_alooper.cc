/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on rtc_task_queue_libevent.cc

#include "rtc_base/task_queue.h"

#include <android/looper.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <list>

#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_rtc_task_queue_alooper_jni/jni/LooperTaskQueueHelper_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace rtc {

namespace {

pthread_key_t g_queue_ptr_tls = 0;

void InitializeTls() {
  RTC_CHECK(pthread_key_create(&g_queue_ptr_tls, nullptr) == 0);
}

pthread_key_t GetQueuePtrTls() {
  static pthread_once_t init_once = PTHREAD_ONCE_INIT;
  RTC_CHECK(pthread_once(&init_once, &InitializeTls) == 0);
  return g_queue_ptr_tls;
}

static const char kQuit = 1;
static const char kRunTask = 2;
static const char kRunReplyTask = 3;

static const int kWakeupEvent = 1;

using Priority = TaskQueue::Priority;

typedef std::function<void(JNIEnv*)> OnWakeupDelayedFunctionType;

// This ignores the SIGPIPE signal on the calling thread.
// This signal can be fired when trying to write() to a pipe that's being
// closed or while closing a pipe that's being written to.
// We can run into that situation (e.g. reply tasks that don't get a chance to
// run because the task queue is being deleted) so we ignore this signal and
// continue as normal.
// As a side note for this implementation, it would be great if we could safely
// restore the sigmask, but unfortunately the operation of restoring it, can
// itself actually cause SIGPIPE to be signaled :-| (e.g. on MacOS)
// The SIGPIPE signal by default causes the process to be terminated, so we
// don't want to risk that.
// An alternative to this approach is to ignore the signal for the whole
// process:
//   signal(SIGPIPE, SIG_IGN);
void IgnoreSigPipeSignalOnCurrentThread() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &sigpipe_mask, nullptr);
}

struct TimerEvent {
  explicit TimerEvent(std::unique_ptr<QueuedTask> task, int64_t when)
      : task(std::move(task)), when(when) {}
  std::unique_ptr<QueuedTask> task;
  // rtc::TimeMillis based timestamp of when the event should be executed.
  int64_t when;
};

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  RTC_CHECK(flags != -1);
  return (flags & O_NONBLOCK) || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

ThreadPriority TaskQueuePriorityToThreadPriority(Priority priority) {
  switch (priority) {
    case Priority::HIGH:
      return kRealtimePriority;
    case Priority::LOW:
      return kLowPriority;
    case Priority::NORMAL:
      return kNormalPriority;
    default:
      RTC_NOTREACHED();
      break;
  }
  return kNormalPriority;
}
}  // namespace

class TaskQueue::Impl : public RefCountInterface {
 public:
  explicit Impl(const char* queue_name,
                TaskQueue* queue,
                Priority priority = Priority::NORMAL);
  ~Impl() override;

  static TaskQueue::Impl* Current();
  static TaskQueue* CurrentQueue();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  void PostTask(std::unique_ptr<QueuedTask> task);
  void PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                        std::unique_ptr<QueuedTask> reply,
                        TaskQueue::Impl* reply_queue);

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

 private:
  static void ThreadMain(void* context);
  static int OnWakeup(int socket, int events, void* context);
  static void OnWakeupDelayed(JNIEnv* env);

  class ReplyTaskOwner;
  class PostAndReplyTask;
  class SetTimerTask;

  typedef RefCountedObject<ReplyTaskOwner> ReplyTaskOwnerRef;

  void PrepareReplyTask(scoped_refptr<ReplyTaskOwnerRef> reply_task);

  struct QueueContext;

  OnWakeupDelayedFunctionType on_delayed_wakeup_function_;
  const webrtc::ScopedJavaGlobalRef<jobject> looper_helper_;

  TaskQueue* const queue_;
  int wakeup_pipe_in_ = -1;
  int wakeup_pipe_out_ = -1;
  rtc::Event ready_event_;
  ALooper* alooper_;
  PlatformThread thread_;
  rtc::CriticalSection pending_lock_;
  std::list<std::unique_ptr<QueuedTask>> pending_ RTC_GUARDED_BY(pending_lock_);
  std::list<scoped_refptr<ReplyTaskOwnerRef>> pending_replies_
      RTC_GUARDED_BY(pending_lock_);
  // List of pending timer events sorted by |when|.
  std::list<TimerEvent> pending_timer_events_ RTC_GUARDED_BY(pending_lock_);
};

struct TaskQueue::Impl::QueueContext {
  explicit QueueContext(TaskQueue::Impl* q) : queue(q) {}
  TaskQueue::Impl* queue;
};

// Posting a reply task is tricky business. This class owns the reply task
// and a reference to it is held by both the reply queue and the first task.
// Here's an outline of what happens when dealing with a reply task.
// * The ReplyTaskOwner owns the |reply_| task.
// * One ref owned by PostAndReplyTask
// * One ref owned by the reply TaskQueue
// * ReplyTaskOwner has a flag |run_task_| initially set to false.
// * ReplyTaskOwner has a method: HasOneRef() (provided by RefCountedObject).
// * After successfully running the original |task_|, PostAndReplyTask() calls
//   set_should_run_task(). This sets |run_task_| to true.
// * In PostAndReplyTask's dtor:
//   * It releases its reference to ReplyTaskOwner (important to do this first).
//   * Sends (write()) a kRunReplyTask message to the reply queue's pipe.
// * PostAndReplyTask doesn't care if write() fails, but when it does:
//   * The reply queue is gone.
//   * ReplyTaskOwner has already been deleted and the reply task too.
// * If write() succeeds:
//   * ReplyQueue receives the kRunReplyTask message
//   * Goes through all pending tasks, finding the first that HasOneRef()
//   * Calls ReplyTaskOwner::Run()
//     * if set_should_run_task() was called, the reply task will be run
//   * Release the reference to ReplyTaskOwner
//   * ReplyTaskOwner and associated |reply_| are deleted.
class TaskQueue::Impl::ReplyTaskOwner {
 public:
  explicit ReplyTaskOwner(std::unique_ptr<QueuedTask> reply)
      : reply_(std::move(reply)) {}

  void Run() {
    RTC_DCHECK(reply_);
    if (run_task_) {
      if (!reply_->Run())
        reply_.release();
    }
    reply_.reset();
  }

  void set_should_run_task() {
    RTC_DCHECK(!run_task_);
    run_task_ = true;
  }

 private:
  std::unique_ptr<QueuedTask> reply_;
  bool run_task_ = false;
};

class TaskQueue::Impl::PostAndReplyTask : public QueuedTask {
 public:
  PostAndReplyTask(std::unique_ptr<QueuedTask> task,
                   std::unique_ptr<QueuedTask> reply,
                   TaskQueue::Impl* reply_queue,
                   int reply_pipe)
      : task_(std::move(task)),
        reply_pipe_(reply_pipe),
        reply_task_owner_(
            new RefCountedObject<ReplyTaskOwner>(std::move(reply))) {
    reply_queue->PrepareReplyTask(reply_task_owner_);
  }

  ~PostAndReplyTask() override {
    reply_task_owner_ = nullptr;
    IgnoreSigPipeSignalOnCurrentThread();
    // Send a signal to the reply queue that the reply task can run now.
    // Depending on whether |set_should_run_task()| was called by the
    // PostAndReplyTask(), the reply task may or may not actually run.
    // In either case, it will be deleted.
    char message = kRunReplyTask;
    RTC_UNUSED(write(reply_pipe_, &message, sizeof(message)));
  }

 private:
  bool Run() override {
    if (!task_->Run())
      task_.release();
    reply_task_owner_->set_should_run_task();
    return true;
  }

  std::unique_ptr<QueuedTask> task_;
  int reply_pipe_;
  scoped_refptr<RefCountedObject<ReplyTaskOwner>> reply_task_owner_;
};

TaskQueue::Impl::Impl(const char* queue_name,
                      TaskQueue* queue,
                      Priority priority /*= NORMAL*/)
    : on_delayed_wakeup_function_(&OnWakeupDelayed),
      looper_helper_(Java_LooperTaskQueueHelper_Constructor(
          webrtc::AttachCurrentThreadIfNeeded(),
          webrtc::NativeToJavaPointer(&on_delayed_wakeup_function_))),
      queue_(queue),
      ready_event_(true /* manual_reset */, false /* initially_signaled */),
      thread_(&TaskQueue::Impl::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  int fds[2];
  RTC_CHECK(pipe(fds) == 0);
  SetNonBlocking(fds[0]);
  SetNonBlocking(fds[1]);
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];

  thread_.Start();
  ready_event_.Wait(rtc::Event::kForever);
  ALooper_addFd(alooper_, wakeup_pipe_out_, kWakeupEvent, ALOOPER_EVENT_INPUT,
                OnWakeup, this);
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());
  struct timespec ts;
  char message = kQuit;
  while (write(wakeup_pipe_in_, &message, sizeof(message)) != sizeof(message)) {
    // The queue is full, so we have no choice but to wait and retry.
    RTC_CHECK_EQ(EAGAIN, errno);
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    nanosleep(&ts, nullptr);
  }

  thread_.Stop();

  IgnoreSigPipeSignalOnCurrentThread();

  close(wakeup_pipe_in_);
  close(wakeup_pipe_out_);
  wakeup_pipe_in_ = -1;
  wakeup_pipe_out_ = -1;
}

// static
TaskQueue::Impl* TaskQueue::Impl::Current() {
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
  return ctx ? ctx->queue : nullptr;
}

// static
TaskQueue* TaskQueue::Impl::CurrentQueue() {
  TaskQueue::Impl* current = Current();
  if (current) {
    return current->queue_;
  }
  return nullptr;
}

bool TaskQueue::Impl::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::Impl::PostTask(std::unique_ptr<QueuedTask> task) {
  RTC_DCHECK(task.get());
  QueuedTask* task_id = task.get();  // Only used for comparison.
  {
    CritScope lock(&pending_lock_);
    pending_.push_back(std::move(task));
  }
  char message = kRunTask;
  if (write(wakeup_pipe_in_, &message, sizeof(message)) != sizeof(message)) {
    RTC_LOG(WARNING) << "Failed to queue task.";
    CritScope lock(&pending_lock_);
    pending_.remove_if([task_id](std::unique_ptr<QueuedTask>& t) {
      return t.get() == task_id;
    });
  }
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  TimerEvent timer_event(std::move(task), rtc::TimeMillis() + milliseconds);

  {
    // This is true if the task will be the first to be executed, in this case
    // we will need to wake the poll loop.
    bool needs_wakeup_ = true;

    CritScope lock(&pending_lock_);
    bool was_inserted_ = false;
    for (auto it = pending_timer_events_.begin();
         it != pending_timer_events_.end(); it++) {
      if (timer_event.when < it->when) {
        pending_timer_events_.insert(it, std::move(timer_event));
        was_inserted_ = true;
        break;
      }
      needs_wakeup_ = false;
    }

    if (!was_inserted_) {
      pending_timer_events_.push_back(std::move(timer_event));
    }

    if (needs_wakeup_) {
      Java_LooperTaskQueueHelper_scheduleWakeup(
          webrtc::AttachCurrentThreadIfNeeded(), looper_helper_, milliseconds);
    }
  }
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  std::unique_ptr<QueuedTask> wrapper_task(
      new PostAndReplyTask(std::move(task), std::move(reply), reply_queue,
                           reply_queue->wakeup_pipe_in_));
  PostTask(std::move(wrapper_task));
}

// static
void TaskQueue::Impl::ThreadMain(void* context) {
  JNIEnv* env = webrtc::AttachCurrentThreadIfNeeded();

  TaskQueue::Impl* me = static_cast<TaskQueue::Impl*>(context);
  Java_LooperTaskQueueHelper_prepare(webrtc::AttachCurrentThreadIfNeeded(),
                                     me->looper_helper_);
  me->alooper_ = ALooper_prepare(0 /* opts */);
  RTC_LOG(LS_INFO) << "Looper created: " << me->alooper_;

  QueueContext queue_context(me);
  pthread_setspecific(GetQueuePtrTls(), &queue_context);
  me->ready_event_.Set();

  Java_LooperTaskQueueHelper_loop(env);

  pthread_setspecific(GetQueuePtrTls(), nullptr);
  me->alooper_ = nullptr;

  RTC_LOG(LS_INFO) << "Looper thread exiting.";
  {
    CritScope lock(&me->pending_lock_);
    me->pending_timer_events_.clear();
  }
}

// static
int TaskQueue::Impl::OnWakeup(int socket, int events, void* context) {
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
  RTC_DCHECK(ctx->queue->wakeup_pipe_out_ == socket);
  char buf;
  RTC_CHECK(sizeof(buf) == read(socket, &buf, sizeof(buf)));
  switch (buf) {
    case kQuit:
      Java_LooperTaskQueueHelper_quit(webrtc::AttachCurrentThreadIfNeeded());
      break;
    case kRunTask: {
      std::unique_ptr<QueuedTask> task;
      {
        CritScope lock(&ctx->queue->pending_lock_);
        RTC_DCHECK(!ctx->queue->pending_.empty());
        task = std::move(ctx->queue->pending_.front());
        ctx->queue->pending_.pop_front();
        RTC_DCHECK(task.get());
      }
      if (!task->Run())
        task.release();
      break;
    }
    case kRunReplyTask: {
      scoped_refptr<ReplyTaskOwnerRef> reply_task;
      {
        CritScope lock(&ctx->queue->pending_lock_);
        for (auto it = ctx->queue->pending_replies_.begin();
             it != ctx->queue->pending_replies_.end(); ++it) {
          if ((*it)->HasOneRef()) {
            reply_task = std::move(*it);
            ctx->queue->pending_replies_.erase(it);
            break;
          }
        }
      }
      reply_task->Run();
      break;
    }
    default:
      RTC_NOTREACHED();
      break;
  }

  return 1;  // Return 1 to continue receiving callbacks.
}

void TaskQueue::Impl::OnWakeupDelayed(JNIEnv* env) {
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));

  while (true) {
    int64_t current_time = rtc::TimeMillis();

    std::unique_ptr<QueuedTask> task_to_run;
    {
      CritScope lock(&ctx->queue->pending_lock_);
      auto it = ctx->queue->pending_timer_events_.begin();

      if (it == ctx->queue->pending_timer_events_.end()) {
        break;
      }

      int64_t delayMs = it->when - current_time;
      if (delayMs <= 0) {
        task_to_run = std::move(it->task);
        ctx->queue->pending_timer_events_.pop_front();
      } else {
        Java_LooperTaskQueueHelper_scheduleWakeup(
            env, ctx->queue->looper_helper_, delayMs);
        break;
      }
    }
    RTC_DCHECK(task_to_run);
    if (!task_to_run->Run())
      task_to_run.release();
  }
}

void TaskQueue::Impl::PrepareReplyTask(
    scoped_refptr<ReplyTaskOwnerRef> reply_task) {
  RTC_DCHECK(reply_task);
  CritScope lock(&pending_lock_);
  pending_replies_.push_back(std::move(reply_task));
}

TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(new RefCountedObject<TaskQueue::Impl>(queue_name, this, priority)) {
}

TaskQueue::~TaskQueue() {}

// static
TaskQueue* TaskQueue::Current() {
  return TaskQueue::Impl::CurrentQueue();
}

// Used for DCHECKing the current queue.
bool TaskQueue::IsCurrent() const {
  return impl_->IsCurrent();
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return TaskQueue::impl_->PostTask(std::move(task));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            reply_queue->impl_.get());
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            impl_.get());
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return TaskQueue::impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc

static void JNI_LooperTaskQueueHelper_OnWakeupDelayed(
    JNIEnv* env,
    const webrtc::JavaParamRef<jclass>& clazz,
    jlong wakeup_ptr) {
  (*reinterpret_cast<rtc::OnWakeupDelayedFunctionType*>(wakeup_ptr))(env);
}
