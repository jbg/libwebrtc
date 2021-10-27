/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef __API_METRONOME_METRONOME_H_
#define __API_METRONOME_METRONOME_H_

#include <memory>
#include <vector>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_EXPORT Metronome {
 public:
  class RTC_EXPORT TickHandle : public rtc::RefCountedNonVirtual<TickHandle> {
   public:
    ~TickHandle() = default;

    bool IsNull() const;
    void Stop();

   protected:
    friend class RTC_EXPORT Metronome;
    TickHandle(TaskQueueBase* task_queue, std::unique_ptr<QueuedTask> task);

   private:
    void Run();

    TaskQueueBase* task_queue_;
    std::unique_ptr<QueuedTask> task_;
  };

  Metronome() = default;
  virtual ~Metronome() = default;

  // Calls task on every metronome tick.
  rtc::scoped_refptr<TickHandle> AddTickListener(
      TaskQueueBase* task_queue,
      std::unique_ptr<QueuedTask> task);

  void RemoveTickListener(rtc::scoped_refptr<TickHandle> handle);

  virtual TimeDelta NextTickDelay() const = 0;

 protected:
  virtual void Start() = 0;
  virtual void Stop() = 0;

  void RunTickTasks();

 private:
  Mutex mutex_;
  std::vector<rtc::scoped_refptr<TickHandle>> tick_handles_
      RTC_GUARDED_BY(mutex_);
};

} // namespace webrtc

#endif  // __API_METRONOME_METRONOME_H_
