/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/scoped_refptr.h"

#if defined(WEBRTC_VIDEO_CAPTURE_DSHOW)
#include "modules/video_capture/windows/video_capture_ds.h"
#endif

#if defined(WEBRTC_VIDEO_CAPTURE_WINRT)
#include "modules/video_capture/windows/device_info_winrt.h"
#include "modules/video_capture/windows/video_capture_winrt.h"
#endif

#include "rtc_base/ref_counted_object.h"

namespace webrtc {
namespace videocapturemodule {

// static
VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {
#if defined(WEBRTC_VIDEO_CAPTURE_DSHOW)
  return DeviceInfoDS::Create();
#endif

#if defined(WEBRTC_VIDEO_CAPTURE_WINRT)
  return DeviceInfoWinRT::Create();
#endif
}

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const char* device_id) {
  if (device_id == nullptr)
    return nullptr;

#if defined(WEBRTC_VIDEO_CAPTURE_DSHOW)
  rtc::scoped_refptr<VideoCaptureDS> capture(
      new rtc::RefCountedObject<VideoCaptureDS>());
#endif

#if defined(WEBRTC_VIDEO_CAPTURE_WINRT)
  rtc::scoped_refptr<VideoCaptureWinRT> capture(
      new rtc::RefCountedObject<VideoCaptureWinRT>());
#endif

  if (capture->Init(device_id) != 0) {
    return nullptr;
  }

  return capture;
}

}  // namespace videocapturemodule
}  // namespace webrtc
