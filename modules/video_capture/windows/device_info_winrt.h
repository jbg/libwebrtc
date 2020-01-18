/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINRT_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINRT_H_

#include "modules/video_capture/device_info_impl.h"

namespace webrtc {
namespace videocapturemodule {

class DeviceInfoWinRT : public DeviceInfoImpl {
 public:
  // Factory function.
  static DeviceInfoWinRT* Create();

  DeviceInfoWinRT();
  ~DeviceInfoWinRT() override;

  int32_t Init() override;
  uint32_t NumberOfDevices() override;

  //
  // Returns the available capture devices.
  //
  int32_t GetDeviceName(uint32_t deviceNumber,
                        char* deviceNameUTF8,
                        uint32_t deviceNameLength,
                        char* deviceUniqueIdUTF8,
                        uint32_t deviceUniqueIdUTF8Length,
                        char* productUniqueIdUTF8,
                        uint32_t productUniqueIdUTF8Length) override;

  //
  // Display OS /capture device specific settings dialog
  //
  int32_t DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                          const char* dialogTitleUTF8,
                                          void* parentWindow,
                                          uint32_t positionX,
                                          uint32_t positionY) override;

  int32_t CreateCapabilityMap(const char* deviceUniqueIdUTF8) override;

 private:
  void* device_info_internal_;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINRT_H_
