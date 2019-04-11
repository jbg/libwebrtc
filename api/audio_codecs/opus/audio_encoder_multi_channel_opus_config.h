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

#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderMultiChannelOpusConfig {
  AudioEncoderOpusConfig single_stream_config;

  // Number of mono/stereo Opus streams.
  int num_streams;

  // Number of channel pairs coupled together, see RFC 7845 section
  // 5.1.1. Has to be less than the number of streams
  int coupled_streams;

  // Channel mapping table, defines the mapping from encoded streams to input
  // channels. See RFC 7845 section 5.1.1.
  std::vector<unsigned char> channel_mapping;

  bool IsOk() const {
    if (!single_stream_config.IsOk()) {
      return false;
    }

    // Check the lengths:
    if (single_stream_config.num_channels < 0 || num_streams < 0 ||
        coupled_streams < 0) {
      return false;
    }
    if (num_streams < coupled_streams) {
      return false;
    }
    if (channel_mapping.size() !=
        static_cast<size_t>(single_stream_config.num_channels)) {
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
    for (size_t i = 0; i < single_stream_config.num_channels; ++i) {
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

    if (single_stream_config.num_channels > 255 || max_coded_channel >= 255) {
      return false;
    }
    return true;
  }
};

}  // namespace webrtc
#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
