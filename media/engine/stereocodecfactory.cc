/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/stereocodecfactory.h"

#include <utility>

#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "modules/video_coding/codecs/stereo/include/stereo_decoder_adapter.h"
#include "modules/video_coding/codecs/stereo/include/stereo_encoder_adapter.h"
#include "rtc_base/logging.h"

namespace webrtc {

StereoEncoderFactory::StereoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> factory)
    : factory_(std::move(factory)) {}

std::vector<SdpVideoFormat> StereoEncoderFactory::GetSupportedFormats() const {
  std::vector<SdpVideoFormat> formats = factory_->GetSupportedFormats();
  formats.emplace_back(cricket::kStereoCodecName);
  return formats;
}

VideoEncoderFactory::CodecInfo StereoEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  if (!cricket::VideoCodec::IsStereoCodec(cricket::VideoCodec(format)))
    return factory_->QueryVideoEncoder(format);
  VideoEncoderFactory::CodecInfo info;
  info.has_internal_source = false;
  info.is_hardware_accelerated = false;
  return info;
}

std::unique_ptr<VideoEncoder> StereoEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  if (!cricket::VideoCodec::IsStereoCodec(cricket::VideoCodec(format)))
    return factory_->CreateVideoEncoder(format);
  return std::unique_ptr<VideoEncoder>(
      new StereoEncoderAdapter(factory_.get()));
}

StereoDecoderFactory::StereoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> factory)
    : factory_(std::move(factory)) {}

std::vector<SdpVideoFormat> StereoDecoderFactory::GetSupportedFormats() const {
  std::vector<SdpVideoFormat> formats = factory_->GetSupportedFormats();
  formats.emplace_back(cricket::kStereoCodecName);
  return formats;
}

std::unique_ptr<VideoDecoder> StereoDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  if (!cricket::VideoCodec::IsStereoCodec(cricket::VideoCodec(format)))
    return factory_->CreateVideoDecoder(format);
  return std::unique_ptr<VideoDecoder>(
      new StereoDecoderAdapter(factory_.get()));
}

}  // namespace webrtc
