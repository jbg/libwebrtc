/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderMultiChannelOpusConfig {
  // Opus API allows a min bitrate of 500bps, but Opus documentation suggests
  // bitrate should be in the range of 6000 to 510000, inclusive.
  static constexpr int kMinBitrateBps = 6000;
  static constexpr int kMaxBitrateBps = 510000;

  int frame_size_ms;
  size_t num_channels;
  enum class ApplicationMode { kVoip, kAudio };
  ApplicationMode application;
  int bitrate_bps;
  bool fec_enabled;
  bool cbr_enabled;
  bool dtx_enabled;
  int max_playback_rate_hz;
  std::vector<int> supported_frame_lengths_ms;

  int complexity;
  int low_rate_complexity;

  // Number of mono/stereo Opus streams.
  int num_streams;

  // Number of channel pairs coupled together, see RFC 7845 section
  // 5.1.1. Has to be less than the number of streams
  int coupled_streams;

  // Channel mapping table, defines the mapping from encoded streams to input
  // channels. See RFC 7845 section 5.1.1.
  std::vector<unsigned char> channel_mapping;

  bool IsOk() const {
    if (frame_size_ms <= 0 || frame_size_ms % 10 != 0)
      return false;
    if (num_channels < 0 || num_channels >= 255) {
      return false;
    }
    if (bitrate_bps < kMinBitrateBps || bitrate_bps > kMaxBitrateBps)
      return false;
    if (complexity < 0 || complexity > 10)
      return false;
    if (low_rate_complexity < 0 || low_rate_complexity > 10)
      return false;
    return true;

    // Check the lengths:
    if (num_channels < 0 || num_streams < 0 || coupled_streams < 0) {
      return false;
    }
    if (num_streams < coupled_streams) {
      return false;
    }
    if (channel_mapping.size() != static_cast<size_t>(num_channels)) {
      return false;
    }

    // Every mono stream codes one channel, every coupled stream codes two. This
    // is the total coded channel count:
    const int max_coded_channel = num_streams + coupled_streams;
    for (const auto& x : channel_mapping) {
      // Coded channels >= max_coded_channel don't exist. Except for 255, which
      // tells Opus to ignore input channel x.
      if (x >= max_coded_channel && x != 255) {
        return false;
      }
    }

    // Inverse mapping.
    constexpr int kNotSet = -1;
    std::vector<int> coded_channels_to_input_channels(max_coded_channel,
                                                      kNotSet);
    for (size_t i = 0; i < num_channels; ++i) {
      if (channel_mapping[i] == 255) {
        continue;
      }

      // If it's not ignored, put it in the inverted mapping. But first check if
      // we've told Opus to use another input channel for this coded channel:
      const int coded_channel = channel_mapping[i];
      if (coded_channels_to_input_channels[coded_channel] != kNotSet) {
        // Coded channel `coded_channel` comes from both input channels
        // `coded_channels_to_input_channels[coded_channel]` and `i`.
        return false;
      }

      coded_channels_to_input_channels[coded_channel] = i;
    }

    // Check that we specified what input the encoder should use to produce
    // every coded channel.
    for (int i = 0; i < max_coded_channel; ++i) {
      if (coded_channels_to_input_channels[i] == kNotSet) {
        // Coded channel `i` has unspecified input channel.
        return false;
      }
    }

    if (num_channels > 255 || max_coded_channel >= 255) {
      return false;
    }
    return true;
  }
};

}  // namespace webrtc
#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
