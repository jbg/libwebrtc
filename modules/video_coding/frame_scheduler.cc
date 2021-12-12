/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_scheduler.h"

#include <algorithm>
#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

constexpr TimeDelta kMaxAllowedFrameDelay = TimeDelta::Millis(5);

// Default value for the maximum decode queue size that is used when the
// low-latency renderer is used.
constexpr size_t kZeroPlayoutDelayDefaultMaxDecodeQueueSize = 8;

}  // namespace

// Helper class which will execute FrameScheduler::OnTimeToReleaseNextFrame
// but also supports cancelation using TaskSafety. This task may be cancelled
// in the case that a better frame was receieved while waiting to decode an
// outstanding frame.
class FrameScheduler::ExtractNextFrameTask {
 public:
  ExtractNextFrameTask(FrameScheduler* scheduler, uint32_t next_frame_rtp)
      : scheduler_(scheduler), next_frame_rtp_(next_frame_rtp) {}

  uint32_t next_frame_rtp() const { return next_frame_rtp_; }

  auto Task() {
    return ToQueuedTask(task_safety_.flag(), [this] {
      scheduler_->OnTimeToReleaseNextFrame(next_frame_rtp_);
    });
  }

 private:
  FrameScheduler* const scheduler_;
  const uint32_t next_frame_rtp_;
  ScopedTaskSafety task_safety_;
};

FrameScheduler::FrameScheduler(Clock* clock,
                               TaskQueueBase* queue,
                               VCMTiming const* timing,
                               FrameBuffer* frame_buffer,
                               Timeouts timeouts,
                               Callbacks* callbacks)
    : clock_(clock),
      frame_buffer_(frame_buffer),
      timing_(timing),
      timeouts_(timeouts),
      callbacks_(callbacks),
      bookkeeping_queue_(queue),
      zero_playout_delay_max_decode_queue_size_(
          "max_decode_queue_size",
          kZeroPlayoutDelayDefaultMaxDecodeQueueSize) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(frame_buffer_);
  RTC_DCHECK(timing_);
  RTC_DCHECK(bookkeeping_queue_);
  RTC_DCHECK(callbacks_);

  ParseFieldTrial({&zero_playout_delay_max_decode_queue_size_},
                  field_trial::FindFullName("WebRTC-ZeroPlayoutDelay"));
}

FrameScheduler::~FrameScheduler() {
  // Check Stop() was called.
  RTC_DCHECK(!timeout_task_.Running());
  RTC_DCHECK(!next_frame_task_);
}

void FrameScheduler::StartTimeoutTask() {
  RTC_DCHECK(!timeout_task_.Running());
  TimeDelta timeout_delay = TimeoutForNextFrame();
  timeout_ = clock_->CurrentTime() + timeout_delay;
  timeout_task_ =
      RepeatingTaskHandle::DelayedStart(bookkeeping_queue_, timeout_delay,
                                        [this] { return HandleTimeoutTask(); });
}

TimeDelta FrameScheduler::HandleTimeoutTask() {
  Timestamp now = clock_->CurrentTime();
  // `timeout_` is hit and we have timed out. Schedule the next timeout at
  // the timeout delay.
  if (now >= timeout_) {
    TimeDelta timeout_delay = TimeoutForNextFrame();
    timeout_ = now + timeout_delay;
    callbacks_->OnTimeout();
    return timeout_delay;
  }
  // Otherwise, `timeout_` changed since we scheduled a timeout. Reschedule
  // a timeout check.
  return timeout_ - now;
}

void FrameScheduler::Stop() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  timeout_task_.Stop();
  // Cancels all delayed tasks.
  next_frame_task_ = nullptr;
}

void FrameScheduler::ForceKeyFrame() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  force_keyframe_ = true;

  // Restart the timeout task if the current timeout is too long.
  TimeDelta timeout_delay = TimeoutForNextFrame();
  if (clock_->CurrentTime() + timeout_delay < timeout_) {
    timeout_task_.Stop();
    StartTimeoutTask();
  }
}

// Returns the id of the last continuous frame if there is one.
void FrameScheduler::OnFrameBufferChanged() {
  MaybeScheduleNextFrame();
}

void FrameScheduler::OnReadyForNextFrame() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  if (!timeout_task_.Running()) {
    StartTimeoutTask();
  }
  if (next_frame_task_) {
    next_frame_task_ = nullptr;
  }
  receiver_ready_for_next_frame_ = true;
  MaybeScheduleNextFrame();
}

void FrameScheduler::MaybeScheduleNextFrame() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  if (!receiver_ready_for_next_frame_)
    return;
  // Skip rescheduling if a task is waiting on the same frame.
  if (next_frame_task_ &&
      next_frame_task_->next_frame_rtp() ==
          frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
    return;
  }

  // If the frame buffer is empty then it has been cleared or is empty.
  // Invalidate outstanding task and return.
  if (!frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
    next_frame_task_ = nullptr;
    return;
  }

  if (force_keyframe_) {
    TryForceKeyframe();
    return;
  }

  // Find which frame should be decoded.
  auto next_frame = SelectNextDecodableFrame();
  if (!next_frame)
    return;
  uint32_t next_rtp;
  TimeDelta max_wait = TimeDelta::Zero();
  std::tie(next_rtp, max_wait) = *next_frame;
  next_frame_task_ = std::make_unique<ExtractNextFrameTask>(this, next_rtp);
  TimeDelta wait = std::max(TimeDelta::Zero(), max_wait);
  bookkeeping_queue_->PostDelayedTask(next_frame_task_->Task(), wait.ms());
}

void FrameScheduler::OnTimeToReleaseNextFrame(uint32_t frame_rtp) {
  RTC_DCHECK_EQ(
      frame_rtp,
      frame_buffer_->NextDecodableTemporalUnitRtpTimestamp().value_or(-1));
  auto frames = frame_buffer_->ExtractNextDecodableTemporalUnit();
  RTC_DCHECK_EQ(frame_rtp, frames.front()->Timestamp());
  YieldReadyFrames(std::move(frames));
}

void FrameScheduler::TryForceKeyframe() {
  // Iterate through the frame buffer until there is a complete keyframe and
  // release this right away.
  while (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp().has_value()) {
    auto next_frame = frame_buffer_->ExtractNextDecodableTemporalUnit();
    if (next_frame.empty()) {
      RTC_DCHECK_NOTREACHED()
          << "Frame buffer should always return at least 1 frame.";
      continue;
    }
    // Found keyframe - decode right away.
    if (next_frame.front()->is_keyframe()) {
      YieldReadyFrames(std::move(next_frame));
      return;
    }
  }
}

void FrameScheduler::YieldReadyFrames(
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) {
  Timestamp now = clock_->CurrentTime();
  const EncodedFrame& first_frame = *frames.front();
  const auto rtp = first_frame.Timestamp();
  const auto render_time = timing_->RenderTimeMs(rtp, now.ms());

  for (auto& frame : frames) {
    frame->SetRenderTime(render_time);
  }
  if (first_frame.is_keyframe())
    force_keyframe_ = false;

  // Extend timeout.
  timeout_ = clock_->CurrentTime() + TimeoutForNextFrame();
  last_released_frame_rtp_ = rtp;
  receiver_ready_for_next_frame_ = false;
  callbacks_->OnFrameReady(std::move(frames));
}

bool FrameScheduler::FrameRtpTimestampsOutOfOrder(uint32_t rtp) const {
  return last_released_frame_rtp_ && AheadOf(*last_released_frame_rtp_, rtp);
}

bool FrameScheduler::TooManyFramesQueued() const {
  return frame_buffer_->CurrentSize() >
         zero_playout_delay_max_decode_queue_size_;
}

absl::optional<std::pair<uint32_t, TimeDelta>>
FrameScheduler::SelectNextDecodableFrame() {
  const Timestamp now = clock_->CurrentTime();
  // Drop temporal units until we don't skip a frame.
  while (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
    auto next_rtp = *frame_buffer_->NextDecodableTemporalUnitRtpTimestamp();

    if (FrameRtpTimestampsOutOfOrder(next_rtp)) {
      frame_buffer_->DropNextDecodableTemporalUnit();
      continue;
    }

    // Current frame with given rtp might be decodable.
    int64_t render_time = timing_->RenderTimeMs(next_rtp, now.ms());
    TimeDelta max_wait = TimeDelta::Millis(
        timing_->MaxWaitingTime(render_time, now.ms(), TooManyFramesQueued()));

    // If the delay is not too far in the past, or this is the last decodable
    // frame then it is the best frame to be decoded. Otherwise, fast-forward
    // to the next frame in the buffer.
    if (max_wait > -kMaxAllowedFrameDelay ||
        next_rtp == frame_buffer_->LastDecodableTemporalUnitRtpTimestamp()) {
      RTC_DLOG(LS_VERBOSE) << "Selected frame with rtp " << next_rtp
                           << " render time " << render_time
                           << " with a max wait of " << max_wait.ms() << "ms";
      return std::make_pair(next_rtp, max_wait);
    }
    RTC_DLOG(LS_VERBOSE) << "Fast-forwarded frame " << next_rtp
                         << " render time " << render_time << " with delay "
                         << max_wait.ms() << "ms";
    frame_buffer_->DropNextDecodableTemporalUnit();
  }

  RTC_DLOG(LS_VERBOSE) << "No frame frame to decode.";
  return absl::nullopt;
}

TimeDelta FrameScheduler::TimeoutForNextFrame() const {
  return force_keyframe_ ? timeouts_.max_wait_for_keyframe
                         : timeouts_.max_wait_for_frame;
}

}  // namespace webrtc
