/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internaldecoderfactory.h"

#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::vector<SdpVideoFormat> InternalDecoderFactory::GetSupportedFormats()
    const {
  std::vector<SdpVideoFormat> formats;
  formats.push_back(SdpVideoFormat(cricket::kVp8CodecName));
  if (VP9Decoder::IsSupported())
    formats.push_back(SdpVideoFormat(cricket::kVp9CodecName));
  if (H264Decoder::IsSupported())
    formats.push_back(SdpVideoFormat(cricket::kH264CodecName));
  return formats;
}

std::unique_ptr<VideoDecoder> InternalDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  if (format.name == cricket::kVp8CodecName)
    return std::unique_ptr<VideoDecoder>(VP8Decoder::Create());

  if (format.name == cricket::kVp9CodecName) {
    RTC_DCHECK(VP9Decoder::IsSupported());
    return std::unique_ptr<VideoDecoder>(VP9Decoder::Create());
  }

  if (format.name == cricket::kH264CodecName) {
    RTC_DCHECK(H264Decoder::IsSupported());
    return std::unique_ptr<VideoDecoder>(H264Decoder::Create());
  }

  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
