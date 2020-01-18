/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/windows/help_functions_winrt.h"

using namespace winrt;
using namespace Windows::Media::MediaProperties;

namespace webrtc {
namespace videocapturemodule {

uint32_t SafelyComputeMediaRatio(MediaRatio mediaRatio) {
  uint32_t denominator = mediaRatio.Denominator();
  if (denominator == 0) {
    return 0;
  }

  return mediaRatio.Numerator() / denominator;
}

VideoType ToVideoType(const hstring& hs) {
  if (hs == L"I420")
    return VideoType::kI420;
  if (hs == L"IYUV")
    return VideoType::kIYUV;
  if (hs == L"RGB24")
    return VideoType::kRGB24;
  if (hs == L"ABGR")
    return VideoType::kABGR;
  if (hs == L"ARGB")
    return VideoType::kARGB;
  if (hs == L"ARGB4444")
    return VideoType::kARGB4444;
  if (hs == L"RGB565")
    return VideoType::kRGB565;
  if (hs == L"RGB565")
    return VideoType::kRGB565;
  if (hs == L"ARGB1555")
    return VideoType::kARGB1555;
  if (hs == L"YUY2")
    return VideoType::kYUY2;
  if (hs == L"YV12")
    return VideoType::kYV12;
  if (hs == L"UYVY")
    return VideoType::kUYVY;
  if (hs == L"MJPEG")
    return VideoType::kMJPEG;
  if (hs == L"NV21")
    return VideoType::kNV21;
  if (hs == L"NV12")
    return VideoType::kNV12;
  if (hs == L"BGRA")
    return VideoType::kBGRA;
  return VideoType::kUnknown;
}

hstring FromVideoType(const VideoType& videoType) {
  switch (videoType) {
    case VideoType::kI420:
      return L"I420";
    case VideoType::kIYUV:
      return L"IYUV";
    case VideoType::kRGB24:
      return L"RGB24";
    case VideoType::kABGR:
      return L"ABGR";
    case VideoType::kARGB:
      return L"ARGB";
    case VideoType::kARGB4444:
      return L"ARGB4444";
    case VideoType::kRGB565:
      return L"RGB565";
    case VideoType::kARGB1555:
      return L"ARGB1555";
    case VideoType::kYUY2:
      return L"YUY2";
    case VideoType::kYV12:
      return L"YV12";
    case VideoType::kUYVY:
      return L"UYVY";
    case VideoType::kMJPEG:
      return L"MJPEG";
    case VideoType::kNV21:
      return L"NV21";
    case VideoType::kNV12:
      return L"NV12";
    case VideoType::kBGRA:
      return L"BGRA";
    default:
      return L"Unknown";
  }
}

inline bool operator==(const webrtc::VideoType& lhs, const hstring& rhs) {
  return FromVideoType(lhs) == rhs;
}

inline bool operator!=(const webrtc::VideoType& lhs, const hstring& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const hstring& lhs, const webrtc::VideoType& rhs) {
  return rhs == lhs;
}

inline bool operator!=(const hstring& lhs, const webrtc::VideoType& rhs) {
  return !(rhs == lhs);
}

}  // namespace videocapturemodule
}  // namespace webrtc
