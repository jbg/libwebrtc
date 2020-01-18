/*
 *  Copyright (c) 2020 The WebRTC project authors:: All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_WINRT_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_WINRT_H_

#include <winrt/base.h>
#include <winrt/Windows.Media.MediaProperties.h>

#include "modules/video_capture/video_capture_defines.h"

namespace webrtc {
namespace videocapturemodule {

uint32_t SafelyComputeMediaRatio(
    winrt::Windows::Media::MediaProperties::MediaRatio);

VideoType ToVideoType(const winrt::hstring&);
winrt::hstring FromVideoType(const VideoType&);

inline bool operator==(const VideoType&, const winrt::hstring&);
inline bool operator!=(const VideoType&, const winrt::hstring&);
inline bool operator==(const winrt::hstring&, const VideoType&);
inline bool operator!=(const winrt::hstring&, const VideoType&);

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_WINRT_H_
