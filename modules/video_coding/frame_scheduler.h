/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_SCHEDULER_H_
#define MODULES_VIDEO_CODING_FRAME_SCHEDULER_H_

#include <memory>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/frame_buffer3.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Schedules frames received on the network for decoding.
//
// Frames are released for decoding via the Callbacks::OnFrameReady method.
// In the case that the stream has no decodable frame for a prolonged period,
// the receiver will be informed via the Callbacks::OnTimeout method. The
// duration of these timeouts are set with the Timeouts struct.
//
// The frame scheduler uses a FrameBuffer to determine which frame should be
// released next. The scheduler checks the frame buffer to determine the best
// frame to be decoded when OnFrameBufferChanged is called. This frame is then
// scheduled to be released in the future. When released, the frame is removed
// from the frame buffer and forwarded to the receiver via
// Callbacks::OnFrameReady. If, while waiting to release a frame, a better frame
// appears in the frame buffer, this better frame will be scheduled for release
// instead.
//
// FrameScheduler runs on a single sequence, which must be the same as the task
// queue provided in the constructor.
class FrameScheduler {
 public:
  class Callback {
   public:
    virtual ~Callback() = default;
    virtual void OnFrameReady(
        absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4>) = 0;

    virtual void OnTimeout() = 0;
  };

  struct Timeouts {
    TimeDelta max_wait_for_keyframe;
    TimeDelta max_wait_for_frame;
  };

  FrameScheduler(Clock* clock,
                 TaskQueueBase* task_queue,
                 VCMTiming const* timing,
                 FrameBuffer* frame_buffer,
                 const Timeouts& timeouts,
                 Callback* callbacks);
  virtual ~FrameScheduler();
  FrameScheduler(const FrameScheduler&) = delete;
  FrameScheduler& operator=(const FrameScheduler&) = delete;

  // Signals to the scheduler that the receiver is ready to decode a new frame.
  // The schedule will not release a frame until this method is called.
  void OnReadyForNextFrame();

  // Stops the frame buffer, in preperation for destruction. Calling
  // OnReadyForNextFrame() after Stop() is not supported.
  void Stop();

  // Forces the next frame returned to be a keyframe.
  void ForceKeyFrame();

  // Informs the scheduler that the frame buffer has changed - either a new
  // frame was inserted or the frame buffer was cleared. The scheduler will
  // schedule the best decodable frame in the case there is one.
  void OnFrameBufferUpdated();

 private:
  TimeDelta TimeoutForNextFrame() const RTC_RUN_ON(bookkeeping_queue_);
  void StartTimeoutTask() RTC_RUN_ON(bookkeeping_queue_);
  TimeDelta HandleTimeoutTask() RTC_RUN_ON(bookkeeping_queue_);

  void MaybeScheduleNextFrame() RTC_RUN_ON(bookkeeping_queue_);
  void TryForceKeyframe() RTC_RUN_ON(bookkeeping_queue_);

  // Returns the RTP timestamp and maximum wait time of the next frame to be
  // decoded. This will skip frames that have out-of-order RTP timestamps, and
  // fast-forward past frames that are too far in the past.
  absl::optional<std::pair<uint32_t, TimeDelta>> SelectNextDecodableFrame()
      RTC_RUN_ON(bookkeeping_queue_);
  bool IsTimestampOlderThanLastDecoded(uint32_t rtp) const
      RTC_RUN_ON(bookkeeping_queue_);
  bool IsTooManyFramesQueued() const RTC_RUN_ON(bookkeeping_queue_);

  void YieldReadyFrames(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames)
      RTC_RUN_ON(bookkeeping_queue_);

  Clock* const clock_;
  FrameBuffer* const frame_buffer_;
  VCMTiming const* const timing_;
  const Timeouts timeouts_;
  Callback* const callback_;
  TaskQueueBase* const bookkeeping_queue_;

  // Initial frame will always be forced as a keyframe.
  bool force_keyframe_ RTC_GUARDED_BY(bookkeeping_queue_) = true;
  absl::optional<uint32_t> next_frame_rtp_ RTC_GUARDED_BY(bookkeeping_queue_);
  absl::optional<uint32_t> last_released_frame_rtp_
      RTC_GUARDED_BY(bookkeeping_queue_);
  bool receiver_ready_for_next_frame_ RTC_GUARDED_BY(bookkeeping_queue_) =
      false;

  RepeatingTaskHandle timeout_task_ RTC_GUARDED_BY(bookkeeping_queue_);
  Timestamp timeout_ RTC_GUARDED_BY(bookkeeping_queue_) =
      Timestamp::MinusInfinity();

  // Maximum number of frames in the decode queue to allow pacing. If the
  // queue grows beyond the max limit, pacing will be disabled and frames will
  // be pushed to the decoder as soon as possible. This only has an effect
  // when the low-latency rendering path is active, which is indicated by
  // the frame's render time == 0.
  FieldTrialParameter<unsigned> zero_playout_delay_max_decode_queue_size_;

  ScopedTaskSafetyDetached task_safety_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_SCHEDULER_H_
