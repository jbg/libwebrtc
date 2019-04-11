/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_multi_channel_opus.h"
#include "api/audio_codecs/opus/audio_encoder_multi_channel_opus_config.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_encoder_fuzzer.h"

namespace webrtc {
namespace {}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  const std::vector<SdpAudioFormat> sdp_formats = {
      SdpAudioFormat({"multiopus",
                      48000,
                      6,
                      {{"minptime", "10"},
                       {"useinbandfec", "1"},
                       {"channel_mapping", "0,4,1,2,3,5"},
                       {"num_streams", "4"},
                       {"coupled_streams", "2"}}}),  // 5.1 surround.

      SdpAudioFormat({"multiopus",
                      48000,
                      1,
                      {{"minptime", "10"},
                       {"useinbandfec", "1"},
                       {"channel_mapping", "0"},
                       {"num_streams", "1"},
                       {"coupled_streams", "0"}}}),  // Mono.

      SdpAudioFormat({"multiopus",
                      48000,
                      8,
                      {{"minptime", "10"},
                       {"useinbandfec", "1"},
                       {"channel_mapping", "0,6,1,2,3,4,5,7"},
                       {"num_streams", "5"},
                       {"coupled_streams", "3"}}})  // 7.1
  };
  if (size == 0) {
    return;
  }
  auto format = sdp_formats[data[0] % sdp_formats.size()];
  absl::optional<AudioEncoderMultiChannelOpus::Config> encoder_config =
      AudioEncoderMultiChannelOpus::SdpToConfig(format);
  RTC_CHECK(encoder_config);
  encoder_config->single_stream_config.frame_size_ms = 20;
  RTC_CHECK(encoder_config->IsOk());

  constexpr int kPayloadType = 100;
  std::unique_ptr<AudioEncoder> enc =
      AudioEncoderMultiChannelOpus::MakeAudioEncoder(*encoder_config,
                                                     kPayloadType);
  FuzzAudioEncoder(rtc::ArrayView<const uint8_t>(data, size), enc.get());
}

}  // namespace webrtc
