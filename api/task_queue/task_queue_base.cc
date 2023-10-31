/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_base.h"

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/functional/any_invocable.h"
#include "api/make_ref_counted.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

#if defined(ABSL_HAVE_THREAD_LOCAL)

namespace webrtc {
namespace {

ABSL_CONST_INIT thread_local TaskQueueBase* current = nullptr;

rtc::FinalRefCountedObject<TaskQueueBase::Voucher>*& CurrentVoucherStorage() {
  static thread_local rtc::FinalRefCountedObject<TaskQueueBase::Voucher>*
      storage = nullptr;
  return storage;
}

}  // namespace

TaskQueueBase::Voucher::ScopedSetter::ScopedSetter(Ptr voucher)
    : old_current_(Voucher::Current()) {
  Voucher::SetCurrent(std::move(voucher));
}

TaskQueueBase::Voucher::ScopedSetter::~ScopedSetter() {
  Voucher::SetCurrent(std::move(old_current_));
}

TaskQueueBase::Voucher::Annex::Id TaskQueueBase::Voucher::Annex::GetNextId() {
  static std::atomic<TaskQueueBase::Voucher::Annex::Id> current_id = 0;
  auto id = current_id.fetch_add(1);
  RTC_CHECK(id < TaskQueueBase::Voucher::kAnnexCapacity);
  return id;
}

TaskQueueBase::Voucher::Ptr
TaskQueueBase::Voucher::CurrentOrCreateForCurrentTask() {
  auto& storage = CurrentVoucherStorage();
  TaskQueueBase::Voucher::Ptr result(storage);
  if (!result) {
    result = rtc::make_ref_counted<TaskQueueBase::Voucher>();
    storage = result.get();
    storage->AddRef();
  }
  return result;
}

TaskQueueBase::Voucher::Ptr TaskQueueBase::Voucher::Current() {
  auto& storage = CurrentVoucherStorage();
  TaskQueueBase::Voucher::Ptr result(storage);
  return result;
}

TaskQueueBase::Voucher::Voucher()
    : annex_(TaskQueueBase::Voucher::kAnnexCapacity) {}

TaskQueueBase::Voucher::~Voucher() = default;

void TaskQueueBase::Voucher::SetCurrent(TaskQueueBase::Voucher::Ptr value) {
  auto& storage = CurrentVoucherStorage();
  if (value.get() != storage) {
    if (storage) {
      storage->Release();
    }
    storage = value.release();
  }
}

TaskQueueBase::Voucher::Annex* TaskQueueBase::Voucher::GetAnnex(Annex::Id id) {
  RTC_CHECK(id < kAnnexCapacity);
  MutexLock lock(&mu_);
  return annex_[id].get();
}

void TaskQueueBase::Voucher::SetAnnex(Annex::Id id,
                                      std::unique_ptr<Annex> annex) {
  RTC_CHECK(id < kAnnexCapacity);
  MutexLock lock(&mu_);
  annex_[id] = std::move(annex);
}

TaskQueueBase* TaskQueueBase::Current() {
  return current;
}

void TaskQueueBase::PostTask(absl::AnyInvocable<void() &&> task,
                             const Location& location) {
  PostTaskInternal(std::move(task), PostTaskTraits{}, location);
}

void TaskQueueBase::PostTaskInternal(absl::AnyInvocable<void() &&> task,
                                     const PostTaskTraits& traits,
                                     const Location& location) {
  auto current = Voucher::Current();
  PostTaskImpl(
      [task = std::move(task), current = std::move(current)]() mutable {
        Voucher::ScopedSetter setter(std::move(current));
        std::move(task)();
      },
      traits, location);
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(current) {
  current = task_queue;
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  current = previous_;
}
}  // namespace webrtc

#elif defined(WEBRTC_POSIX)

#include <pthread.h>

namespace webrtc {
namespace {

ABSL_CONST_INIT pthread_key_t g_queue_ptr_tls = 0;

void InitializeTls() {
  RTC_CHECK(pthread_key_create(&g_queue_ptr_tls, nullptr) == 0);
}

pthread_key_t GetQueuePtrTls() {
  static pthread_once_t init_once = PTHREAD_ONCE_INIT;
  RTC_CHECK(pthread_once(&init_once, &InitializeTls) == 0);
  return g_queue_ptr_tls;
}

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return static_cast<TaskQueueBase*>(pthread_getspecific(GetQueuePtrTls()));
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(TaskQueueBase::Current()) {
  pthread_setspecific(GetQueuePtrTls(), task_queue);
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  pthread_setspecific(GetQueuePtrTls(), previous_);
}

}  // namespace webrtc

#else
#error Unsupported platform
#endif
