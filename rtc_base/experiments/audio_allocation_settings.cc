/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "audio_allocation_settings.h"
#include "rtc_base/experiments/audio_allocation_settings.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
// Based on min bitrate for Opus codec.
constexpr DataRate kDefaultMinEncoderBitrate = DataRate::KilobitsPerSec<6>();
constexpr DataRate kDefaultMaxEncoderBitrate = DataRate::KilobitsPerSec<32>();

// TODO(srte): Remove this when we have proper overhead calculation in place.
DataRate LegacyOverheadBitrate() {
  // OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
  constexpr DataSize kOverheadPerPacket = DataSize::Bytes<20 + 8 + 10 + 12>();
  const TimeDelta kMaxFrameLength = TimeDelta::ms(120);
  return kOverheadPerPacket / kMaxFrameLength;
}
}  // namespace

AudioAllocationSettings::AudioAllocationSettings()
    : legacy_audio_send_side_bwe_trial_(
          field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")),
      legacy_allocate_audio_without_feedback_trial_(
          field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")),
      legacy_audio_only_call_(legacy_audio_send_side_bwe_trial_ &&
                              !legacy_allocate_audio_without_feedback_trial_),
      register_rtcp_observer_(
          field_trial::IsEnabled("WebRTC-Audio-RegisterRtcpObserver")),
      enable_alr_probing_(
          field_trial::IsEnabled("WebRTC-Audio-EnableAlrProbing")),
      send_transport_sequence_numbers_(
          field_trial::IsEnabled("WebRTC-Audio-SendTransportSequenceNumbers")),
      include_in_acknowledged_estimate_(
          field_trial::IsEnabled("WebRTC-Audio-AddSentToAckedEstimate")),
      default_min_bitrate_("min"),
      default_max_bitrate_("max"),
      min_overhead_("min_oh", DataRate::Zero()),
      max_overhead_("max_oh", DataRate::Zero()) {
  ParseFieldTrial({&default_min_bitrate_, &default_max_bitrate_, &min_overhead_,
                   &max_overhead_},
                  field_trial::FindFullName("WebRTC-Audio-AllocationOverride"));
}

AudioAllocationSettings::~AudioAllocationSettings() {}

bool AudioAllocationSettings::SendTransportSequenceNumber() const {
  return legacy_audio_only_call_ || send_transport_sequence_numbers_;
}

bool AudioAllocationSettings::AlwaysIncludeAudioInAllocation() const {
  return legacy_allocate_audio_without_feedback_trial_ ||
         include_in_acknowledged_estimate_;
}

bool AudioAllocationSettings::RegisterRtcpObserver() const {
  return register_rtcp_observer_ || legacy_audio_only_call_;
}

bool AudioAllocationSettings::EnableAlrProbing() const {
  return enable_alr_probing_ || legacy_audio_only_call_;
}

absl::optional<DataRate> AudioAllocationSettings::DefaultMinBitrate() const {
  if (legacy_audio_send_side_bwe_trial_)
    return kDefaultMinEncoderBitrate;
  return default_min_bitrate_.GetOptional();
}

absl::optional<DataRate> AudioAllocationSettings::DefaultMaxBitrate() const {
  if (legacy_audio_send_side_bwe_trial_)
    return kDefaultMaxEncoderBitrate;
  return default_max_bitrate_.GetOptional();
}

DataRate AudioAllocationSettings::MinOverheadBitrate() const {
  if (legacy_audio_send_side_bwe_trial_) {
    return LegacyOverheadBitrate();
  }
  return min_overhead_;
}

DataRate AudioAllocationSettings::MaxOverheadBitrate() const {
  if (legacy_audio_send_side_bwe_trial_) {
    return LegacyOverheadBitrate();
  }
  return max_overhead_;
}

}  // namespace webrtc
