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

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/checks.h"
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

namespace frame_scheduler_impl {

FrameReleaseScheduler::FrameReleaseScheduler(
    Clock* clock,
    webrtc::VCMTiming const* timing,
    TaskQueueBase* const bookkeeping_queue,
    FrameReleaseCallback callback)
    : clock_(clock),
      bookkeeping_queue_(bookkeeping_queue),
      timing_(timing),
      callback_(std::move(callback)) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(bookkeeping_queue_);
  RTC_DCHECK(timing_);
}

FrameReleaseScheduler::~FrameReleaseScheduler() {
  RTC_DCHECK(!scheduled_rtp_) << "Outstanding scheduled rtp=" << *scheduled_rtp_
                              << ". Should call "
                                 "CancelScheduledFrames before destruction.";
}

FrameReleaseScheduler::Action FrameReleaseScheduler::MaybeScheduleFrame(
    uint32_t next_temporal_unit_rtp,
    uint32_t last_temporal_unit_rtp,
    bool too_many_frames_queued) {
  // Frame already scheduled.
  if (scheduled_rtp_ == next_temporal_unit_rtp)
    return Action::kFrameScheduled;

  const Timestamp now = clock_->CurrentTime();
  int64_t render_time = timing_->RenderTimeMs(next_temporal_unit_rtp, now.ms());
  TimeDelta max_wait = TimeDelta::Millis(
      timing_->MaxWaitingTime(render_time, now.ms(), too_many_frames_queued));

  // If the delay is not too far in the past, or this is the last decodable
  // frame then it is the best frame to be decoded. Otherwise, fast-forward
  // to the next frame in the buffer.
  if (max_wait > -kMaxAllowedFrameDelay ||
      next_temporal_unit_rtp == last_temporal_unit_rtp) {
    RTC_DLOG(LS_VERBOSE) << "Selected frame with rtp " << next_temporal_unit_rtp
                         << " render time " << render_time
                         << " with a max wait of " << max_wait.ms() << "ms";
    ScheduleFrameForRelease(next_temporal_unit_rtp, max_wait);
    return Action::kFrameScheduled;
  }
  RTC_DLOG(LS_VERBOSE) << "Fast-forwarded frame " << next_temporal_unit_rtp
                       << " render time " << render_time << " with delay "
                       << max_wait.ms() << "ms";
  return Action::kDropFrame;
}

void FrameReleaseScheduler::CancelScheduledFrames() {
  scheduled_rtp_ = absl::nullopt;
}

void FrameReleaseScheduler::ScheduleFrameForRelease(uint32_t rtp,
                                                    TimeDelta max_wait) {
  if (scheduled_rtp_ == rtp)
    return;
  scheduled_rtp_ = rtp;
  TimeDelta wait = std::max(TimeDelta::Zero(), max_wait);
  bookkeeping_queue_->PostDelayedTask(
      ToQueuedTask(task_safety_.flag(),
                   [this, rtp] {
                     RTC_DCHECK_RUN_ON(bookkeeping_queue_);
                     // If the next frame rtp has changed since this task was
                     // posted, a new frame was scheduled for extraction and
                     // this scheduled release should be skipped.
                     if (scheduled_rtp_ != rtp)
                       return;
                     scheduled_rtp_ = absl::nullopt;
                     callback_(rtp);
                   }),
      wait.ms());
}

StreamTimeoutTracker::StreamTimeoutTracker(
    Clock* clock,
    TaskQueueBase* const bookkeeping_queue,
    const Timeouts& timeouts,
    TimeoutCallback callback)
    : clock_(clock),
      bookkeeping_queue_(bookkeeping_queue),
      timeouts_(timeouts),
      callback_(std::move(callback)) {}

StreamTimeoutTracker::~StreamTimeoutTracker() {
  RTC_DCHECK(!timeout_task_.Running());
}

bool StreamTimeoutTracker::Running() const {
  return timeout_task_.Running();
}

void StreamTimeoutTracker::Start(bool waiting_for_keyframe) {
  RTC_DCHECK(!timeout_task_.Running());
  waiting_for_keyframe_ = waiting_for_keyframe;
  TimeDelta timeout_delay = TimeoutForNextFrame();
  timeout_ = clock_->CurrentTime() + timeout_delay;
  timeout_task_ = RepeatingTaskHandle::DelayedStart(
      bookkeeping_queue_, timeout_delay, [this] {
        RTC_DCHECK_RUN_ON(bookkeeping_queue_);
        return HandleTimeoutTask();
      });
}

void StreamTimeoutTracker::Stop() {
  RTC_DCHECK(timeout_task_.Running());
  timeout_task_.Stop();
}

void StreamTimeoutTracker::SetWaitingForKeyframe() {
  waiting_for_keyframe_ = true;
  TimeDelta timeout_delay = TimeoutForNextFrame();
  if (clock_->CurrentTime() + timeout_delay < timeout_) {
    Stop();
    Start(waiting_for_keyframe_);
  }
}

void StreamTimeoutTracker::OnEncodedFrameReleased() {
  // If we were waiting for a keyframe, then it has just been released.
  waiting_for_keyframe_ = false;
  timeout_ = clock_->CurrentTime() + TimeoutForNextFrame();
}

TimeDelta StreamTimeoutTracker::HandleTimeoutTask() {
  Timestamp now = clock_->CurrentTime();
  // `timeout_` is hit and we have timed out. Schedule the next timeout at
  // the timeout delay.
  if (now >= timeout_) {
    TimeDelta timeout_delay = TimeoutForNextFrame();
    timeout_ = now + timeout_delay;
    callback_();
    return timeout_delay;
  }
  // Otherwise, `timeout_` changed since we scheduled a timeout. Reschedule
  // a timeout check.
  return timeout_ - now;
}

}  // namespace frame_scheduler_impl

FrameScheduler::FrameScheduler(Clock* clock,
                               TaskQueueBase* queue,
                               VCMTiming const* timing,
                               FrameBuffer* frame_buffer,
                               const Timeouts& timeouts,
                               Callback* callback)
    : clock_(clock),
      frame_buffer_(frame_buffer),
      timing_(timing),
      callback_(callback),
      bookkeeping_queue_(queue),
      scheduler_(
          clock_,
          timing,
          bookkeeping_queue_,
          absl::bind_front(&FrameScheduler::OnFrameReadyForRelease, this)),
      timeout_tracker_(
          clock_,
          bookkeeping_queue_,
          {timeouts.max_wait_for_keyframe, timeouts.max_wait_for_frame},
          absl::bind_front(&Callback::OnTimeout, callback_)),
      zero_playout_delay_max_decode_queue_size_(
          "max_decode_queue_size",
          kZeroPlayoutDelayDefaultMaxDecodeQueueSize) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(frame_buffer_);
  RTC_DCHECK(bookkeeping_queue_);
  RTC_DCHECK(callback_);

  ParseFieldTrial({&zero_playout_delay_max_decode_queue_size_},
                  field_trial::FindFullName("WebRTC-ZeroPlayoutDelay"));
}

void FrameScheduler::Stop() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  timeout_tracker_.Stop();
  scheduler_.CancelScheduledFrames();
}

void FrameScheduler::ForceKeyFrame() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  force_keyframe_ = true;
  timeout_tracker_.SetWaitingForKeyframe();
}

void FrameScheduler::OnFrameBufferUpdated() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  MaybeScheduleNextFrame();
}

void FrameScheduler::OnReadyForNextFrame() {
  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  if (!timeout_tracker_.Running()) {
    timeout_tracker_.Start(force_keyframe_);
  }
  receiver_ready_for_next_frame_ = true;
  MaybeScheduleNextFrame();
}

void FrameScheduler::MaybeScheduleNextFrame() {
  if (!receiver_ready_for_next_frame_)
    return;
  // If the frame buffer is empty then it has been cleared or is empty.
  // Cancel all scheduled frames.
  if (!frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
    scheduler_.CancelScheduledFrames();
    return;
  }

  if (force_keyframe_) {
    TryForceKeyframe();
    return;
  }
  ScheduleNextDecodableFrame();
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

void FrameScheduler::OnFrameReadyForRelease(uint32_t rtp) {
  if (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp() != rtp) {
    RTC_DCHECK_NOTREACHED()
        << "Frame buffer and scheduled were out of sync - wrong RTP scheduled: "
        << " Scheduled=" << rtp << " Next Frame RTP="
        << frame_buffer_->NextDecodableTemporalUnitRtpTimestamp().value_or(0);
    return;
  }

  RTC_DCHECK_RUN_ON(bookkeeping_queue_);
  YieldReadyFrames(frame_buffer_->ExtractNextDecodableTemporalUnit());
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
  last_released_frame_rtp_ = rtp;
  receiver_ready_for_next_frame_ = false;
  timeout_tracker_.OnEncodedFrameReleased();
  callback_->OnFrameReady(std::move(frames));
}

bool FrameScheduler::IsTimestampOlderThanLastDecoded(uint32_t rtp) const {
  return last_released_frame_rtp_ && AheadOf(*last_released_frame_rtp_, rtp);
}

bool FrameScheduler::IsTooManyFramesQueued() const {
  return frame_buffer_->CurrentSize() >
         zero_playout_delay_max_decode_queue_size_;
}

void FrameScheduler::ScheduleNextDecodableFrame() {
  // Drop temporal units until we don't skip a frame.
  while (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
    auto next_rtp = *frame_buffer_->NextDecodableTemporalUnitRtpTimestamp();
    RTC_DCHECK(frame_buffer_->LastDecodableTemporalUnitRtpTimestamp());
    auto last_rtp = *frame_buffer_->LastDecodableTemporalUnitRtpTimestamp();

    if (IsTimestampOlderThanLastDecoded(next_rtp)) {
      frame_buffer_->DropNextDecodableTemporalUnit();
      continue;
    }

    switch (scheduler_.MaybeScheduleFrame(next_rtp, last_rtp,
                                          IsTooManyFramesQueued())) {
      case frame_scheduler_impl::FrameReleaseScheduler::Action::kDropFrame:
        frame_buffer_->DropNextDecodableTemporalUnit();
        break;
      case frame_scheduler_impl::FrameReleaseScheduler::Action::kFrameScheduled:
        return;
    }
  }
}

}  // namespace webrtc
