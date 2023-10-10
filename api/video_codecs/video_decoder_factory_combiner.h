/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_COMBINER_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_COMBINER_H_

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "api/field_trials_view.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_item.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Combines multiple factories represented as VideoDecoderFactorItem into single
// one and represent it as VideoDecoderFactory.
// Creates decoder using 1st factory that support passed format.
class RTC_EXPORT VideoDecoderFactoryCombiner : public VideoDecoderFactory {
 public:
  explicit VideoDecoderFactoryCombiner(
      std::vector<std::vector<VideoDecoderFactoryItem>> formats);
  explicit VideoDecoderFactoryCombiner(
      std::vector<std::vector<VideoDecoderFactoryItem>> formats,
      const FieldTrialsView& field_trials ABSL_ATTRIBUTE_LIFETIME_BOUND);

  VideoDecoderFactoryCombiner(const VideoDecoderFactoryCombiner&) = delete;
  VideoDecoderFactoryCombiner& operator=(const VideoDecoderFactoryCombiner&) =
      delete;

  ~VideoDecoderFactoryCombiner() = default;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

 private:
  void ConstructFromItems(
      std::vector<std::vector<VideoDecoderFactoryItem>> items);

  // Returns index of the matching format, or formats_.size() if none matches.
  size_t Find(const SdpVideoFormat& format) const;

  const FieldTrialBasedConfig default_field_trials_;
  const FieldTrialsView& field_trials_;

  // formats_ and factores_ have 1:1 matching: factories_[i] creates Decoder for
  // formats_[i]. They kept as two separate vectors to make
  // `GetSupportedFormats` implementation simple.
  std::vector<SdpVideoFormat> formats_;
  std::vector<
      std::function<std::unique_ptr<VideoDecoder>(const FieldTrialsView&)>>
      factories_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_COMBINER_H_
