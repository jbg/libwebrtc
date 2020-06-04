/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kMaxActiveComparisons = 10;
constexpr int kFreezeThresholdMs = 150;
constexpr int kMicrosPerSecond = 1000000;
constexpr int kBitsInByte = 8;

void LogFrameCounters(const std::string& name, const FrameCounters& counters) {
  RTC_LOG(INFO) << "[" << name << "] Captured    : " << counters.captured;
  RTC_LOG(INFO) << "[" << name << "] Pre encoded : " << counters.pre_encoded;
  RTC_LOG(INFO) << "[" << name << "] Encoded     : " << counters.encoded;
  RTC_LOG(INFO) << "[" << name << "] Received    : " << counters.received;
  RTC_LOG(INFO) << "[" << name << "] Rendered    : " << counters.rendered;
  RTC_LOG(INFO) << "[" << name << "] Dropped     : " << counters.dropped;
}

void LogStreamInternalStats(const std::string& name, const StreamStats& stats) {
  RTC_LOG(INFO) << "[" << name
                << "] Dropped by encoder     : " << stats.dropped_by_encoder;
  RTC_LOG(INFO) << "[" << name << "] Dropped before encoder : "
                << stats.dropped_before_encoder;
}

}  // namespace

void RateCounter::AddEvent(Timestamp event_time) {
  if (event_first_time_.IsMinusInfinity()) {
    event_first_time_ = event_time;
  }
  event_last_time_ = event_time;
  event_count_++;
}

double RateCounter::GetEventsPerSecond() const {
  RTC_DCHECK(!IsEmpty());
  // Divide on us and multiply on kMicrosPerSecond to correctly process cases
  // where there were too small amount of events, so difference is less then 1
  // sec. We can use us here, because Timestamp has us resolution.
  return static_cast<double>(event_count_) /
         (event_last_time_ - event_first_time_).us() * kMicrosPerSecond;
}

bool operator<(const StatsKey& a, const StatsKey& b) {
  if (a.stream_label != b.stream_label) {
    return a.stream_label < b.stream_label;
  }
  if (a.sender != b.sender) {
    return a.sender < b.sender;
  }
  return a.receiver < b.receiver;
}

bool operator==(const StatsKey& a, const StatsKey& b) {
  return a.stream_label == b.stream_label && a.sender == b.sender &&
         a.receiver == b.receiver;
}

DefaultVideoQualityAnalyzer::DefaultVideoQualityAnalyzer(
    bool heavy_metrics_computation_enabled,
    int max_frames_in_flight_per_stream_count)
    : heavy_metrics_computation_enabled_(heavy_metrics_computation_enabled),
      max_frames_in_flight_per_stream_count_(
          max_frames_in_flight_per_stream_count),
      clock_(Clock::GetRealTimeClock()) {}
DefaultVideoQualityAnalyzer::~DefaultVideoQualityAnalyzer() {
  Stop();
}

void DefaultVideoQualityAnalyzer::Start(
    std::string test_case_name,
    rtc::ArrayView<const std::string> peer_names,
    int max_threads_count) {
  test_label_ = std::move(test_case_name);
  for (int i = 0; i < static_cast<int>(peer_names.size()); ++i) {
    peer_to_index_.insert({peer_names[i], i});
    peer_by_index_.insert({i, peer_names[i]});
  }
  for (int i = 0; i < max_threads_count; i++) {
    auto thread = std::make_unique<rtc::PlatformThread>(
        &DefaultVideoQualityAnalyzer::ProcessComparisonsThread, this,
        ("DefaultVideoQualityAnalyzerWorker-" + std::to_string(i)).data(),
        rtc::ThreadPriority::kNormalPriority);
    thread->Start();
    thread_pool_.push_back(std::move(thread));
  }
  {
    rtc::CritScope crit(&lock_);
    RTC_CHECK(start_time_.IsMinusInfinity());

    state_ = State::kActive;
    start_time_ = Now();
  }
  StartMeasuringCpuProcessTime();
}

uint16_t DefaultVideoQualityAnalyzer::OnFrameCaptured(
    absl::string_view peer_name,
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  // |next_frame_id| is atomic, so we needn't lock here.
  uint16_t frame_id = next_frame_id_++;
  Timestamp start_time = Timestamp::MinusInfinity();
  int peer_index = peer_to_index_.at(std::string(peer_name));
  {
    rtc::CritScope crit(&lock_);
    // Create a local copy of start_time_ to access it under
    // |comparison_lock_| without holding a |lock_|
    start_time = start_time_;
  }
  {
    // Ensure stats for this stream exists.
    rtc::CritScope crit(&comparison_lock_);
    // TODO(titovartem) maybe we need to make it faster...
    for (auto& peer_entry : peer_to_index_) {
      if (peer_index == peer_entry.second) {
        continue;
      }
      StatsKey stats_key(stream_label, std::string(peer_name),
                         peer_entry.first);
      if (stream_stats_.find(stats_key) == stream_stats_.end()) {
        stream_stats_.insert({stats_key, StreamStats()});
        // Assume that the first freeze was before first stream frame captured.
        // This way time before the first freeze would be counted as time
        // between freezes.
        stream_last_freeze_end_time_.insert({stats_key, start_time});
      }
    }
  }
  {
    rtc::CritScope crit(&lock_);
    stream_to_sender_[stream_label] = std::string(peer_name);
    frame_counters_.captured++;
    for (auto& peer_entry : peer_by_index_) {
      stream_frame_counters_[stream_label][peer_entry.first].captured++;
    }

    auto state_it = stream_states_.find(stream_label);
    if (state_it == stream_states_.end()) {
      stream_states_.insert(
          {stream_label, StreamState(peer_index, peer_to_index_.size())});
    }
    StreamState* state = &stream_states_.at(stream_label);
    state->PushBack(frame_id);
    // Update frames in flight info.
    auto it = captured_frames_in_flight_.find(frame_id);
    if (it != captured_frames_in_flight_.end()) {
      // We overflow uint16_t and hit previous frame id and this frame is
      // still in flight. It means that this stream wasn't rendered for long
      // time and we need to process existing frame as dropped.
      for (auto& peer_entry : peer_to_index_) {
        const std::string& receiver = peer_entry.first;
        const int receiver_index = peer_entry.second;
        uint16_t oldest_frame_id = state->PopFront(receiver_index);
        RTC_DCHECK_EQ(frame_id, oldest_frame_id);
        frame_counters_.dropped++;
        stream_frame_counters_[stream_label][receiver_index].dropped++;
        AddComparison(StatsKey(stream_label, std::string(peer_name), receiver),
                      it->second.frame(), absl::nullopt, true,
                      it->second.GetStatsForPeer(receiver_index));
      }

      captured_frames_in_flight_.erase(it);
    }
    captured_frames_in_flight_.emplace(
        frame_id, FrameInFlight(stream_label, frame, /*captured_time=*/Now(),
                                peer_to_index_.size()));
    // Set frame id on local copy of the frame
    captured_frames_in_flight_.at(frame_id).SetFrameId(frame_id);

    // Update history stream<->frame mapping
    for (auto it = stream_to_frame_id_history_.begin();
         it != stream_to_frame_id_history_.end(); ++it) {
      it->second.erase(frame_id);
    }
    stream_to_frame_id_history_[stream_label].insert(frame_id);

    // If state has too many frames that are in flight => remove the oldest
    // queued frame in order to avoid to use too much memory.
    if (state->GetAliveFramesCount() > max_frames_in_flight_per_stream_count_) {
      uint16_t frame_id_to_remove = state->MarkNextAliveFrameAsDead();
      auto it = captured_frames_in_flight_.find(frame_id_to_remove);
      RTC_DCHECK(it != captured_frames_in_flight_.end())
          << "Alive frame not found";
      bool is_removed = it->second.RemoveFrame();
      RTC_DCHECK(is_removed)
          << "Invalid stream state: alive frame is removed already";
    }
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::OnFramePreEncode(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  auto it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(it != captured_frames_in_flight_.end())
      << "Frame id=" << frame.id() << " not found";
  frame_counters_.pre_encoded++;
  for (auto& peer_entry : peer_by_index_) {
    stream_frame_counters_[it->second.stream_label()][peer_entry.first]
        .pre_encoded++;
  }
  it->second.SetPreEncodeTime(Now());
}

void DefaultVideoQualityAnalyzer::OnFrameEncoded(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image,
    const EncoderStats& stats) {
  rtc::CritScope crit(&lock_);
  auto it = captured_frames_in_flight_.find(frame_id);
  RTC_DCHECK(it != captured_frames_in_flight_.end());
  // For SVC we can receive multiple encoded images for one frame, so to cover
  // all cases we have to pick the last encode time.
  if (it->second.encoded_time().IsInfinite()) {
    // Increase counters only when we meet this frame first time.
    frame_counters_.encoded++;
    for (auto& peer_entry : peer_by_index_) {
      stream_frame_counters_[it->second.stream_label()][peer_entry.first]
          .encoded++;
    }
  }
  it->second.OnFrameEncoded(Now(), encoded_image.size(),
                            stats.target_encode_bitrate);
}

void DefaultVideoQualityAnalyzer::OnFrameDropped(
    absl::string_view peer_name,
    webrtc::EncodedImageCallback::DropReason reason) {
  // Here we do nothing, because we will see this drop on renderer side.
}

void DefaultVideoQualityAnalyzer::OnFramePreDecode(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& input_image) {
  rtc::CritScope crit(&lock_);
  auto it = captured_frames_in_flight_.find(frame_id);
  RTC_DCHECK(it != captured_frames_in_flight_.end());

  int peer_index = peer_to_index_.at(std::string(peer_name));
  RTC_DCHECK(!it->second.has_received_time(peer_index))
      << "Received multiple spatial layers for stream_label="
      << it->second.stream_label();
  frame_counters_.received++;
  stream_frame_counters_[it->second.stream_label()][peer_index].received++;
  it->second.SetDecodeStartTime(peer_index, Now());
  // Determine the time of the last received packet of this video frame.
  RTC_DCHECK(!input_image.PacketInfos().empty());
  int64_t last_receive_time =
      std::max_element(input_image.PacketInfos().cbegin(),
                       input_image.PacketInfos().cend(),
                       [](const RtpPacketInfo& a, const RtpPacketInfo& b) {
                         return a.receive_time_ms() < b.receive_time_ms();
                       })
          ->receive_time_ms();
  it->second.SetReceivedTime(peer_index, Timestamp::Millis(last_receive_time));
}

void DefaultVideoQualityAnalyzer::OnFrameDecoded(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    const DecoderStats& stats) {
  rtc::CritScope crit(&lock_);
  auto it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(it != captured_frames_in_flight_.end());
  frame_counters_.decoded++;
  int peer_index = peer_to_index_.at(std::string(peer_name));
  stream_frame_counters_[it->second.stream_label()][peer_index].decoded++;
  it->second.SetDecodeEndTime(peer_index, Now());
}

void DefaultVideoQualityAnalyzer::OnFrameRendered(
    absl::string_view peer_name,
    const webrtc::VideoFrame& raw_frame) {
  // Copy entire video frame including video buffer to ensure that analyzer
  // won't hold any WebRTC internal buffers.
  VideoFrame frame = raw_frame;
  frame.set_video_frame_buffer(
      I420Buffer::Copy(*raw_frame.video_frame_buffer()->ToI420()));

  int peer_index = peer_to_index_.at(std::string(peer_name));

  rtc::CritScope crit(&lock_);
  // Find corresponding captured frame.
  auto frame_it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(frame_it != captured_frames_in_flight_.end());
  FrameInFlight* frame_in_flight = &frame_it->second;
  absl::optional<VideoFrame> captured_frame = frame_in_flight->frame();
  // Update frames counters.
  frame_counters_.rendered++;
  stream_frame_counters_[frame_in_flight->stream_label()][peer_index]
      .rendered++;

  // Update current frame stats.
  frame_in_flight->OnFrameRendered(peer_index, Now(), frame.width(),
                                   frame.height());

  // After we received frame here we need to check if there are any dropped
  // frames between this one and last one, that was rendered for this video
  // stream.

  const std::string& stream_label = frame_in_flight->stream_label();
  StreamState* state = &stream_states_.at(stream_label);
  const StatsKey stats_key(stream_label, peer_by_index_[state->owner()],
                           std::string(peer_name));
  int dropped_count = 0;
  while (!state->Empty() && state->Front(peer_index) != frame.id()) {
    dropped_count++;
    uint16_t dropped_frame_id = state->PopFront(peer_index);
    // Frame with id |dropped_frame_id| was dropped. We need:
    // 1. Update global and stream frame counters
    // 2. Extract corresponding frame from |captured_frames_in_flight_|
    // 3. Send extracted frame to comparison with dropped=true
    // . Cleanup dropped frame
    frame_counters_.dropped++;
    stream_frame_counters_[stream_label][peer_index].dropped++;

    auto dropped_frame_it = captured_frames_in_flight_.find(dropped_frame_id);
    RTC_DCHECK(dropped_frame_it != captured_frames_in_flight_.end());
    absl::optional<VideoFrame> dropped_frame = dropped_frame_it->second.frame();

    AddComparison(stats_key, dropped_frame, absl::nullopt, true,
                  dropped_frame_it->second.GetStatsForPeer(peer_index));

    if (dropped_frame_it->second.DoesAllPeersReceived()) {
      captured_frames_in_flight_.erase(dropped_frame_it);
    }
  }
  RTC_DCHECK(!state->Empty());
  state->PopFront(peer_index);

  if (state->last_rendered_frame_time(peer_index)) {
    frame_in_flight->SetPrevFrameRenderedTime(
        peer_index, state->last_rendered_frame_time(peer_index).value());
  }
  state->set_last_rendered_frame_time(
      peer_index, frame_in_flight->rendered_time(peer_index));
  {
    rtc::CritScope cr(&comparison_lock_);
    stream_stats_[stats_key].skipped_between_rendered.AddSample(dropped_count);
  }
  AddComparison(stats_key, captured_frame, frame, false,
                frame_in_flight->GetStatsForPeer(peer_index));

  if (frame_it->second.DoesAllPeersReceived()) {
    captured_frames_in_flight_.erase(frame_it);
  }
}

void DefaultVideoQualityAnalyzer::OnEncoderError(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Encoder error for frame.id=" << frame.id()
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::OnDecoderError(absl::string_view peer_name,
                                                 uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Decoder error for frame_id=" << frame_id
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::Stop() {
  StopMeasuringCpuProcessTime();
  {
    rtc::CritScope crit(&lock_);
    if (state_ == State::kStopped) {
      return;
    }
    state_ = State::kStopped;
  }
  comparison_available_event_.Set();
  for (auto& thread : thread_pool_) {
    thread->Stop();
  }
  // PlatformThread have to be deleted on the same thread, where it was created
  thread_pool_.clear();

  // Perform final Metrics update. On this place analyzer is stopped and no one
  // holds any locks.
  {
    // Time between freezes.
    // Count time since the last freeze to the end of the call as time
    // between freezes.
    rtc::CritScope crit1(&lock_);
    rtc::CritScope crit2(&comparison_lock_);
    for (auto& state_entry : stream_states_) {
      const std::string& stream_label = state_entry.first;
      const StreamState& stream_state = state_entry.second;
      for (auto& peer_entry : peer_to_index_) {
        const std::string& peer_name = peer_entry.first;
        const int peer_index = peer_entry.second;

        if (peer_index == stream_state.owner()) {
          continue;
        }

        StatsKey stats_key(stream_label, peer_by_index_[stream_state.owner()],
                           peer_name);

        // If there are no freezes in the call we have to report
        // time_between_freezes_ms as call duration and in such case
        // |stream_last_freeze_end_time_| for this stream will be |start_time_|.
        // If there is freeze, then we need add time from last rendered frame
        // to last freeze end as time between freezes.
        if (stream_state.last_rendered_frame_time(peer_index)) {
          stream_stats_[stats_key].time_between_freezes_ms.AddSample(
              stream_state.last_rendered_frame_time(peer_index).value().ms() -
              stream_last_freeze_end_time_.at(stats_key).ms());
        }
      }
    }
  }
  ReportResults();
}

std::string DefaultVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  rtc::CritScope crit1(&lock_);
  auto it = captured_frames_in_flight_.find(frame_id);
  if (it != captured_frames_in_flight_.end()) {
    return it->second.stream_label();
  }
  for (auto hist_it = stream_to_frame_id_history_.begin();
       hist_it != stream_to_frame_id_history_.end(); ++hist_it) {
    auto hist_set_it = hist_it->second.find(frame_id);
    if (hist_set_it != hist_it->second.end()) {
      return hist_it->first;
    }
  }
  RTC_CHECK(false) << "Unknown frame_id=" << frame_id;
}

std::set<StatsKey> DefaultVideoQualityAnalyzer::GetKnownVideoStreams() const {
  rtc::CritScope crit2(&comparison_lock_);
  std::set<StatsKey> out;
  for (auto& item : stream_stats_) {
    out.insert(item.first);
  }
  return out;
}

const FrameCounters& DefaultVideoQualityAnalyzer::GetGlobalCounters() const {
  rtc::CritScope crit(&lock_);
  return frame_counters_;
}

std::map<StatsKey, FrameCounters>
DefaultVideoQualityAnalyzer::GetPerStreamCounters() const {
  rtc::CritScope crit(&lock_);
  std::map<StatsKey, FrameCounters> out;
  for (auto& stream_entry : stream_frame_counters_) {
    const std::string& stream_label = stream_entry.first;
    for (auto& receiver_entry : stream_entry.second) {
      const int receiver_index = receiver_entry.first;
      const FrameCounters& counters = receiver_entry.second;

      StatsKey stats_key(stream_label, stream_to_sender_.at(stream_label),
                         peer_by_index_.at(receiver_index));
      out.insert({stats_key, counters});
    }
  }
  return out;
}

std::map<StatsKey, StreamStats> DefaultVideoQualityAnalyzer::GetStats() const {
  rtc::CritScope cri(&comparison_lock_);
  return stream_stats_;
}

AnalyzerStats DefaultVideoQualityAnalyzer::GetAnalyzerStats() const {
  rtc::CritScope crit(&comparison_lock_);
  return analyzer_stats_;
}

void DefaultVideoQualityAnalyzer::AddComparison(
    StatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats) {
  StartExcludingCpuThreadTime();
  rtc::CritScope crit(&comparison_lock_);
  analyzer_stats_.comparisons_queue_size.AddSample(comparisons_.size());
  // If there too many computations waiting in the queue, we won't provide
  // frames itself to make future computations lighter.
  if (comparisons_.size() >= kMaxActiveComparisons) {
    comparisons_.emplace_back(std::move(stats_key), absl::nullopt,
                              absl::nullopt, dropped, frame_stats,
                              OverloadReason::kCpu);
  } else {
    OverloadReason overload_reason = OverloadReason::kNone;
    if (!captured && !dropped) {
      overload_reason = OverloadReason::kMemory;
    }
    comparisons_.emplace_back(std::move(stats_key), std::move(captured),
                              std::move(rendered), dropped, frame_stats,
                              overload_reason);
  }
  comparison_available_event_.Set();
  StopExcludingCpuThreadTime();
}

void DefaultVideoQualityAnalyzer::ProcessComparisonsThread(void* obj) {
  static_cast<DefaultVideoQualityAnalyzer*>(obj)->ProcessComparisons();
}

void DefaultVideoQualityAnalyzer::ProcessComparisons() {
  while (true) {
    // Try to pick next comparison to perform from the queue.
    absl::optional<FrameComparison> comparison = absl::nullopt;
    {
      rtc::CritScope crit(&comparison_lock_);
      if (!comparisons_.empty()) {
        comparison = comparisons_.front();
        comparisons_.pop_front();
        if (!comparisons_.empty()) {
          comparison_available_event_.Set();
        }
      }
    }
    if (!comparison) {
      bool more_frames_expected;
      {
        // If there are no comparisons and state is stopped =>
        // no more frames expected.
        rtc::CritScope crit(&lock_);
        more_frames_expected = state_ != State::kStopped;
      }
      if (!more_frames_expected) {
        comparison_available_event_.Set();
        return;
      }
      comparison_available_event_.Wait(1000);
      continue;
    }

    StartExcludingCpuThreadTime();
    ProcessComparison(comparison.value());
    StopExcludingCpuThreadTime();
  }
}

void DefaultVideoQualityAnalyzer::ProcessComparison(
    const FrameComparison& comparison) {
  // Perform expensive psnr and ssim calculations while not holding lock.
  double psnr = -1.0;
  double ssim = -1.0;
  if (heavy_metrics_computation_enabled_ && comparison.captured &&
      !comparison.dropped) {
    psnr = I420PSNR(&*comparison.captured, &*comparison.rendered);
    ssim = I420SSIM(&*comparison.captured, &*comparison.rendered);
  }

  const FrameStats& frame_stats = comparison.frame_stats;

  rtc::CritScope crit(&comparison_lock_);
  auto stats_it = stream_stats_.find(comparison.stats_key);
  RTC_CHECK(stats_it != stream_stats_.end());
  StreamStats* stats = &stats_it->second;
  analyzer_stats_.comparisons_done++;
  if (comparison.overload_reason == OverloadReason::kCpu) {
    analyzer_stats_.cpu_overloaded_comparisons_done++;
  } else if (comparison.overload_reason == OverloadReason::kMemory) {
    analyzer_stats_.memory_overloaded_comparisons_done++;
  }
  if (psnr > 0) {
    stats->psnr.AddSample(psnr);
  }
  if (ssim > 0) {
    stats->ssim.AddSample(ssim);
  }
  if (frame_stats.encoded_time.IsFinite()) {
    stats->encode_time_ms.AddSample(
        (frame_stats.encoded_time - frame_stats.pre_encode_time).ms());
    stats->encode_frame_rate.AddEvent(frame_stats.encoded_time);
    stats->total_encoded_images_payload += frame_stats.encoded_image_size;
    stats->target_encode_bitrate.AddSample(frame_stats.target_encode_bitrate);
  } else {
    if (frame_stats.pre_encode_time.IsFinite()) {
      stats->dropped_by_encoder++;
    } else {
      stats->dropped_before_encoder++;
    }
  }
  // Next stats can be calculated only if frame was received on remote side.
  if (!comparison.dropped) {
    stats->resolution_of_rendered_frame.AddSample(
        *comparison.frame_stats.rendered_frame_width *
        *comparison.frame_stats.rendered_frame_height);
    stats->transport_time_ms.AddSample(
        (frame_stats.decode_start_time - frame_stats.encoded_time).ms());
    stats->total_delay_incl_transport_ms.AddSample(
        (frame_stats.rendered_time - frame_stats.captured_time).ms());
    stats->decode_time_ms.AddSample(
        (frame_stats.decode_end_time - frame_stats.decode_start_time).ms());
    stats->receive_to_render_time_ms.AddSample(
        (frame_stats.rendered_time - frame_stats.received_time).ms());

    if (frame_stats.prev_frame_rendered_time.IsFinite()) {
      TimeDelta time_between_rendered_frames =
          frame_stats.rendered_time - frame_stats.prev_frame_rendered_time;
      stats->time_between_rendered_frames_ms.AddSample(
          time_between_rendered_frames.ms());
      double average_time_between_rendered_frames_ms =
          stats->time_between_rendered_frames_ms.GetAverage();
      if (time_between_rendered_frames.ms() >
          std::max(kFreezeThresholdMs + average_time_between_rendered_frames_ms,
                   3 * average_time_between_rendered_frames_ms)) {
        stats->freeze_time_ms.AddSample(time_between_rendered_frames.ms());
        auto freeze_end_it =
            stream_last_freeze_end_time_.find(comparison.stats_key);
        RTC_DCHECK(freeze_end_it != stream_last_freeze_end_time_.end());
        stats->time_between_freezes_ms.AddSample(
            (frame_stats.prev_frame_rendered_time - freeze_end_it->second)
                .ms());
        freeze_end_it->second = frame_stats.rendered_time;
      }
    }
  }
}

void DefaultVideoQualityAnalyzer::ReportResults() {
  using ::webrtc::test::ImproveDirection;

  rtc::CritScope crit1(&lock_);
  rtc::CritScope crit2(&comparison_lock_);
  for (auto& item : stream_stats_) {
    ReportResults(GetTestCaseName(StatsKeyToMetricName(item.first)),
                  item.second,
                  stream_frame_counters_.at(item.first.stream_label)
                      .at(peer_to_index_[item.first.receiver]));
  }
  test::PrintResult("cpu_usage", "", test_label_.c_str(), GetCpuUsagePercent(),
                    "%", false, ImproveDirection::kSmallerIsBetter);
  LogFrameCounters("Global", frame_counters_);
  for (auto& item : stream_stats_) {
    LogFrameCounters(item.first.ToString(),
                     stream_frame_counters_.at(item.first.stream_label)
                         .at(peer_to_index_[item.first.receiver]));
    LogStreamInternalStats(item.first.ToString(), item.second);
  }
  if (!analyzer_stats_.comparisons_queue_size.IsEmpty()) {
    RTC_LOG(INFO) << "comparisons_queue_size min="
                  << analyzer_stats_.comparisons_queue_size.GetMin()
                  << "; max=" << analyzer_stats_.comparisons_queue_size.GetMax()
                  << "; 99%="
                  << analyzer_stats_.comparisons_queue_size.GetPercentile(0.99);
  }
  RTC_LOG(INFO) << "comparisons_done=" << analyzer_stats_.comparisons_done;
  RTC_LOG(INFO) << "cpu_overloaded_comparisons_done="
                << analyzer_stats_.cpu_overloaded_comparisons_done;
  RTC_LOG(INFO) << "memory_overloaded_comparisons_done="
                << analyzer_stats_.memory_overloaded_comparisons_done;
}

void DefaultVideoQualityAnalyzer::ReportResults(
    const std::string& test_case_name,
    const StreamStats& stats,
    const FrameCounters& frame_counters) {
  using ::webrtc::test::ImproveDirection;
  TimeDelta test_duration = Now() - start_time_;

  double sum_squared_interframe_delays_secs = 0;
  Timestamp video_start_time = Timestamp::PlusInfinity();
  Timestamp video_end_time = Timestamp::MinusInfinity();
  for (const SamplesStatsCounter::StatsSample& sample :
       stats.time_between_rendered_frames_ms.GetTimedSamples()) {
    double interframe_delay_ms = sample.value;
    const double interframe_delays_secs = interframe_delay_ms / 1000.0;
    // Sum of squared inter frame intervals is used to calculate the harmonic
    // frame rate metric. The metric aims to reflect overall experience related
    // to smoothness of video playback and includes both freezes and pauses.
    sum_squared_interframe_delays_secs +=
        interframe_delays_secs * interframe_delays_secs;
    if (sample.time < video_start_time) {
      video_start_time = sample.time;
    }
    if (sample.time > video_end_time) {
      video_end_time = sample.time;
    }
  }
  double harmonic_framerate_fps = 0;
  TimeDelta video_duration = video_end_time - video_start_time;
  if (sum_squared_interframe_delays_secs > 0.0 && video_duration.IsFinite()) {
    harmonic_framerate_fps = static_cast<double>(video_duration.us()) /
                             static_cast<double>(kMicrosPerSecond) /
                             sum_squared_interframe_delays_secs;
  }

  ReportResult("psnr", test_case_name, stats.psnr, "dB",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("ssim", test_case_name, stats.ssim, "unitless",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("transport_time", test_case_name, stats.transport_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("total_delay_incl_transport", test_case_name,
               stats.total_delay_incl_transport_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_rendered_frames", test_case_name,
               stats.time_between_rendered_frames_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("harmonic_framerate", "", test_case_name,
                    harmonic_framerate_fps, "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  test::PrintResult("encode_frame_rate", "", test_case_name,
                    stats.encode_frame_rate.IsEmpty()
                        ? 0
                        : stats.encode_frame_rate.GetEventsPerSecond(),
                    "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  ReportResult("encode_time", test_case_name, stats.encode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_freezes", test_case_name,
               stats.time_between_freezes_ms, "ms",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("freeze_time_ms", test_case_name, stats.freeze_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("pixels_per_frame", test_case_name,
               stats.resolution_of_rendered_frame, "count",
               ImproveDirection::kBiggerIsBetter);
  test::PrintResult("min_psnr", "", test_case_name,
                    stats.psnr.IsEmpty() ? 0 : stats.psnr.GetMin(), "dB",
                    /*important=*/false, ImproveDirection::kBiggerIsBetter);
  ReportResult("decode_time", test_case_name, stats.decode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("receive_to_render_time", test_case_name,
               stats.receive_to_render_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("dropped_frames", "", test_case_name,
                    frame_counters.dropped, "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  test::PrintResult("frames_in_flight", "", test_case_name,
                    frame_counters.captured - frame_counters.rendered -
                        frame_counters.dropped,
                    "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  ReportResult("max_skipped", test_case_name, stats.skipped_between_rendered,
               "count", ImproveDirection::kSmallerIsBetter);
  ReportResult("target_encode_bitrate", test_case_name,
               stats.target_encode_bitrate / kBitsInByte, "bytesPerSecond",
               ImproveDirection::kNone);
  test::PrintResult(
      "actual_encode_bitrate", "", test_case_name,
      static_cast<double>(stats.total_encoded_images_payload) /
          static_cast<double>(test_duration.us()) * kMicrosPerSecond,
      "bytesPerSecond", /*important=*/false, ImproveDirection::kNone);
}

void DefaultVideoQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

std::string DefaultVideoQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_label_ + "/" + stream_label;
}

Timestamp DefaultVideoQualityAnalyzer::Now() {
  return clock_->CurrentTime();
}

std::string DefaultVideoQualityAnalyzer::StatsKeyToMetricName(
    const StatsKey& key) {
  if (peer_to_index_.size() <= 2) {
    return key.stream_label;
  }
  return key.ToString();
}

void DefaultVideoQualityAnalyzer::StartMeasuringCpuProcessTime() {
  rtc::CritScope lock(&cpu_measurement_lock_);
  cpu_time_ -= rtc::GetProcessCpuTimeNanos();
  wallclock_time_ -= rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzer::StopMeasuringCpuProcessTime() {
  rtc::CritScope lock(&cpu_measurement_lock_);
  cpu_time_ += rtc::GetProcessCpuTimeNanos();
  wallclock_time_ += rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzer::StartExcludingCpuThreadTime() {
  rtc::CritScope lock(&cpu_measurement_lock_);
  cpu_time_ += rtc::GetThreadCpuTimeNanos();
}

void DefaultVideoQualityAnalyzer::StopExcludingCpuThreadTime() {
  rtc::CritScope lock(&cpu_measurement_lock_);
  cpu_time_ -= rtc::GetThreadCpuTimeNanos();
}

double DefaultVideoQualityAnalyzer::GetCpuUsagePercent() {
  rtc::CritScope lock(&cpu_measurement_lock_);
  return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    StatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats,
    OverloadReason overload_reason)
    : stats_key(std::move(stats_key)),
      captured(std::move(captured)),
      rendered(std::move(rendered)),
      dropped(dropped),
      frame_stats(std::move(frame_stats)),
      overload_reason(overload_reason) {}

uint16_t DefaultVideoQualityAnalyzer::StreamState::PopFront(int peer) {
  size_t size_before = frame_ids_.size();
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(peer);
  RTC_DCHECK(frame_id.has_value());
  size_t size_after = frame_ids_.size();
  if ((dead_frames_count_ > 0) && (size_after < size_before)) {
    dead_frames_count_--;
  }
  return frame_id.value();
}

uint16_t DefaultVideoQualityAnalyzer::StreamState::MarkNextAliveFrameAsDead() {
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(owner_);
  RTC_DCHECK(frame_id.has_value());
  dead_frames_count_++;
  return frame_id.value();
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::RemoveFrame() {
  if (!frame_) {
    return false;
  }
  frame_ = absl::nullopt;
  return true;
}
void DefaultVideoQualityAnalyzer::FrameInFlight::SetFrameId(uint16_t id) {
  if (frame_) {
    frame_->set_id(id);
  }
}

std::vector<int>
DefaultVideoQualityAnalyzer::FrameInFlight::GetPeersWhichDidntReceive() {
  std::vector<int> out;
  for (int i = 0; i < receivers_count_; ++i) {
    if (rendered_time_.find(i) == rendered_time_.end()) {
      out.push_back(i);
    }
  }
  return out;
}

DefaultVideoQualityAnalyzer::FrameStats
DefaultVideoQualityAnalyzer::FrameInFlight::GetStatsForPeer(int peer) {
  FrameStats stats(stream_label_, captured_time_, pre_encode_time_,
                   encoded_time_);
  stats.target_encode_bitrate = target_encode_bitrate_;
  stats.encoded_image_size = encoded_image_size_;

  {
    auto it = received_time_.find(peer);
    if (it != received_time_.end()) {
      stats.received_time = it->second;
    }
  }
  {
    auto it = decode_start_time_.find(peer);
    if (it != decode_start_time_.end()) {
      stats.decode_start_time = it->second;
    }
  }
  {
    auto it = decode_end_time_.find(peer);
    if (it != decode_end_time_.end()) {
      stats.decode_end_time = it->second;
    }
  }
  {
    auto it = rendered_time_.find(peer);
    if (it != rendered_time_.end()) {
      stats.rendered_time = it->second;
    }
  }
  {
    auto it = prev_frame_rendered_time_.find(peer);
    if (it != prev_frame_rendered_time_.end()) {
      stats.prev_frame_rendered_time = it->second;
    }
  }
  {
    auto it = rendered_frame_width_.find(peer);
    if (it != rendered_frame_width_.end()) {
      stats.rendered_frame_width = it->second;
    }
  }
  {
    auto it = rendered_frame_height_.find(peer);
    if (it != rendered_frame_height_.end()) {
      stats.rendered_frame_height = it->second;
    }
  }
  return stats;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
