/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_SCHEDULER_H_
#define MODULES_VIDEO_CODING_FRAME_SCHEDULER_H_

#include <stdint.h>

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

namespace frame_scheduler_impl {

class FrameReleaseScheduler {
 public:
  using FrameReleaseCallback = std::function<void(/*rtp_timestamp*/ uint32_t)>;
  FrameReleaseScheduler(Clock* clock,
                        webrtc::VCMTiming const* timing,
                        TaskQueueBase* const bookkeeping_queue,
                        FrameReleaseCallback callback);
  ~FrameReleaseScheduler();
  FrameReleaseScheduler(const FrameReleaseScheduler&) = delete;
  FrameReleaseScheduler& operator=(const FrameReleaseScheduler&) = delete;

  enum class Action {
    kDropFrame,
    kFrameScheduled,
  };

  // TODO(eshr): Remove too_many_frames_queued flag.
  Action MaybeScheduleFrame(uint32_t next_temporal_unit_rtp,
                            uint32_t last_temporal_unit_rtp,
                            bool too_many_frames_queued);

  void CancelScheduledFrames();

 private:
  void ScheduleFrameForRelease(uint32_t rtp, TimeDelta wait);

  Clock* const clock_;
  TaskQueueBase* const bookkeeping_queue_;
  webrtc::VCMTiming const* const timing_;
  const FrameReleaseCallback callback_;
  absl::optional<uint32_t> scheduled_rtp_;
  ScopedTaskSafetyDetached task_safety_;
};

class StreamTimeoutTracker {
 public:
  struct Timeouts {
    TimeDelta max_wait_for_keyframe;
    TimeDelta max_wait_for_frame;
  };

  // TODO(eshr): It could be useful to know what the timeout delay was.
  using TimeoutCallback = std::function<void()>;
  StreamTimeoutTracker(Clock* clock,
                       TaskQueueBase* const bookkeeping_queue,
                       const Timeouts& timeouts,
                       TimeoutCallback callback);
  ~StreamTimeoutTracker();
  StreamTimeoutTracker(const StreamTimeoutTracker&) = delete;
  StreamTimeoutTracker& operator=(const StreamTimeoutTracker&) = delete;

  bool Running() const;
  void Start(bool waiting_for_keyframe);
  void Stop();
  void SetWaitingForKeyframe();
  void OnEncodedFrameReleased();

 private:
  TimeDelta TimeoutForNextFrame() const {
    return waiting_for_keyframe_ ? timeouts_.max_wait_for_keyframe
                                 : timeouts_.max_wait_for_frame;
  }
  TimeDelta HandleTimeoutTask();

  Clock* const clock_;
  TaskQueueBase* const bookkeeping_queue_;
  const Timeouts timeouts_;
  const TimeoutCallback callback_;
  RepeatingTaskHandle timeout_task_;

  Timestamp timeout_ = Timestamp::MinusInfinity();
  bool waiting_for_keyframe_;
};

}  // namespace frame_scheduler_impl

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
  void MaybeScheduleNextFrame() RTC_RUN_ON(bookkeeping_queue_);
  void TryForceKeyframe() RTC_RUN_ON(bookkeeping_queue_);

  // Returns the RTP timestamp and maximum wait time of the next frame to be
  // decoded. This will skip frames that have out-of-order RTP timestamps, and
  // fast-forward past frames that are too far in the past.
  void ScheduleNextDecodableFrame() RTC_RUN_ON(bookkeeping_queue_);
  bool IsTimestampOlderThanLastDecoded(uint32_t rtp) const
      RTC_RUN_ON(bookkeeping_queue_);
  bool IsTooManyFramesQueued() const RTC_RUN_ON(bookkeeping_queue_);

  void OnFrameReadyForRelease(uint32_t rtp);

  void YieldReadyFrames(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames)
      RTC_RUN_ON(bookkeeping_queue_);

  Clock* const clock_;
  FrameBuffer* const frame_buffer_;
  VCMTiming const* const timing_;
  Callback* const callback_;
  TaskQueueBase* const bookkeeping_queue_;

  frame_scheduler_impl::FrameReleaseScheduler scheduler_;
  frame_scheduler_impl::StreamTimeoutTracker timeout_tracker_;

  // Initial frame will always be forced as a keyframe.
  bool force_keyframe_ RTC_GUARDED_BY(bookkeeping_queue_) = true;
  absl::optional<uint32_t> last_released_frame_rtp_
      RTC_GUARDED_BY(bookkeeping_queue_);
  bool receiver_ready_for_next_frame_ RTC_GUARDED_BY(bookkeeping_queue_) =
      false;

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
