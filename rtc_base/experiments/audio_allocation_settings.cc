/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/audio_allocation_settings.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
// Based on min bitrate for Opus codec.
constexpr DataRate kDefaultMinEncoderBitrate = DataRate::KilobitsPerSec<6>();
constexpr DataRate kDefaultMaxEncoderBitrate = DataRate::KilobitsPerSec<32>();

// TODO(srte): Remove these when we have proper overhead calculation in place.
// OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
constexpr DataSize kOverheadPerPacket = DataSize::Bytes<20 + 8 + 10 + 12>();
DataRate MinOverhead() {
  constexpr TimeDelta kMaxFrameLength = TimeDelta::Millis<120>();
  return kOverheadPerPacket / kMaxFrameLength;
};
DataRate MaxOverhead() {
  constexpr TimeDelta kMinFrameLength = TimeDelta::Millis<20>();
  return kOverheadPerPacket / kMinFrameLength;
};
}  // namespace

AudioAllocationSettings::AudioAllocationSettings()
    : legacy_audio_send_side_bwe_trial_(
          field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")),
      legacy_allocate_audio_without_feedback_trial_(
          field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")),
      legacy_audio_only_call_(legacy_audio_send_side_bwe_trial_ &&
                              !legacy_allocate_audio_without_feedback_trial_),
      include_audio_in_feedback_(
          field_trial::IsEnabled("WebRTC-Audio-IncludeInFeedback")),
      allocate_audio_in_video_calls_(
          field_trial::IsEnabled("WebRTC-Audio-IncludeInAllocation")),
      default_min_bitrate_("min", kDefaultMinEncoderBitrate + MinOverhead()),
      default_max_bitrate_("max", kDefaultMaxEncoderBitrate + MaxOverhead()) {
  ParseFieldTrial({&default_min_bitrate_, &default_max_bitrate_},
                  field_trial::FindFullName("WebRTC-Audio-Allocation"));
  // If audio is included in feedback, it has to be part of the allocation,
  // otherwise we will allocate all of the estimated audio+video bitrate to
  // video.
  RTC_DCHECK(!include_audio_in_feedback_ || allocate_audio_in_video_calls_);
}

AudioAllocationSettings::~AudioAllocationSettings() {}

bool AudioAllocationSettings::IncludeAudioInFeedback() const {
  return legacy_audio_only_call_ || include_audio_in_feedback_;
}

bool AudioAllocationSettings::IncludeAudioInAllocation() const {
  return legacy_audio_send_side_bwe_trial_ || allocate_audio_in_video_calls_ ||
         include_audio_in_feedback_;
}

bool AudioAllocationSettings::RegisterRtcpObserver() const {
  return legacy_audio_only_call_;
}

bool AudioAllocationSettings::EnableAlrProbing() const {
  return legacy_audio_only_call_;
}

DataRate AudioAllocationSettings::DefaultMinBitrate() const {
  return default_min_bitrate_;
}

DataRate AudioAllocationSettings::DefaultMaxBitrate() const {
  return default_max_bitrate_;
}

}  // namespace webrtc
