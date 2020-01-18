/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/windows/device_info_winrt.h"

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Media.Capture.h>
#include <winrt/Windows.Media.Devices.h>
#include <winrt/Windows.Media.MediaProperties.h>

#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/windows/help_functions_winrt.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"

using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;
using namespace Windows::Media::Capture;
using namespace Windows::Media::Devices;
using namespace Windows::Media::MediaProperties;

namespace webrtc {
namespace videocapturemodule {

///////////////////////////////////////////////////////////////////////////////
//
// DeviceInfoWinRT
//
///////////////////////////////////////////////////////////////////////////////

// static
DeviceInfoWinRT* DeviceInfoWinRT::Create() {
  return new DeviceInfoWinRT();
}

int32_t DeviceInfoWinRT::Init() {
  return 0;
}

uint32_t DeviceInfoWinRT::NumberOfDevices() {
  ReadLockScoped cs(_apiLock);

  auto deviceInformationFindAllAsync =
      DeviceInformation::FindAllAsync(DeviceClass::VideoCapture);
  blocking_suspend(deviceInformationFindAllAsync);
  if (deviceInformationFindAllAsync.Status() == AsyncStatus::Error) {
    return -1;
  }

  DeviceInformationCollection videoCaptureDevices =
      deviceInformationFindAllAsync.GetResults();
  return videoCaptureDevices.Size();
}

int32_t DeviceInfoWinRT::GetDeviceName(uint32_t deviceNumber,
                                       char* deviceNameUTF8,
                                       uint32_t deviceNameLength,
                                       char* deviceUniqueIdUTF8,
                                       uint32_t deviceUniqueIdUTF8Length,
                                       char* productUniqueIdUTF8,
                                       uint32_t productUniqueIdUTF8Length) {
  ReadLockScoped cs(_apiLock);

  auto deviceInformationFindAllAsync =
      DeviceInformation::FindAllAsync(DeviceClass::VideoCapture);
  blocking_suspend(deviceInformationFindAllAsync);
  if (deviceInformationFindAllAsync.Status() == AsyncStatus::Error) {
    return -1;
  }

  DeviceInformationCollection videoCaptureDevices =
      deviceInformationFindAllAsync.GetResults();
  if (deviceNumber >= videoCaptureDevices.Size()) {
    return -1;
  }

  DeviceInformation videoCaptureDevice =
      videoCaptureDevices.GetAt(deviceNumber);

  if (deviceNameLength > 0) {
    auto videoCaptureDeviceName = videoCaptureDevice.Name();
    int convResult = WideCharToMultiByte(
        CP_UTF8, 0, videoCaptureDeviceName.c_str(), -1, (char*)deviceNameUTF8,
        deviceNameLength, NULL, NULL);
    if (convResult == 0) {
      RTC_LOG(LS_INFO) << "Failed to convert device name to UTF8, error = "
                       << GetLastError();
      return -1;
    }
  }

  auto videoCaptureDeviceId = videoCaptureDevice.Id();

  if (deviceUniqueIdUTF8Length > 0) {
    int convResult = WideCharToMultiByte(
        CP_UTF8, 0, videoCaptureDeviceId.c_str(), -1, (char*)deviceUniqueIdUTF8,
        deviceUniqueIdUTF8Length, NULL, NULL);
    if (convResult == 0) {
      RTC_LOG(LS_INFO) << "Failed to convert device name to UTF8, error = "
                       << GetLastError();
      return -1;
    }
  }

  if (productUniqueIdUTF8Length > 0) {
    using namespace std;

    auto wsDeviceId = wstringstream(videoCaptureDeviceId.c_str());

    wstring wsProductUniqueId;
    if (!getline(wsDeviceId, wsProductUniqueId, L'&') ||
        !getline(wsDeviceId, wsProductUniqueId, L'&')) {
      return -1;
    }

    int convResult = WideCharToMultiByte(CP_UTF8, 0, wsProductUniqueId.c_str(),
                                         -1, (char*)productUniqueIdUTF8,
                                         productUniqueIdUTF8Length, NULL, NULL);
    if (convResult == 0) {
      RTC_LOG(LS_INFO) << "Failed to convert product name to UTF8, error = "
                       << GetLastError();
      return -1;
    }
  }

  if (deviceNameLength) {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " " << deviceNameUTF8;
  }

  return 0;
}

int32_t DeviceInfoWinRT::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8,
    void* parentWindow,
    uint32_t positionX,
    uint32_t positionY) {
  return -1;
}

int32_t DeviceInfoWinRT::CreateCapabilityMap(const char* deviceUniqueIdUTF8) {
  // Checks if deviceUniqueIdUTF8 length is not too long
  const int32_t deviceUniqueIdUTF8Length = (int32_t)strnlen(
      (char*)deviceUniqueIdUTF8, kVideoCaptureUniqueNameLength);

  if (deviceUniqueIdUTF8Length == kVideoCaptureUniqueNameLength) {
    RTC_LOG(LS_INFO) << "Device ID too long";
    return -1;
  }

  RTC_LOG(LS_INFO) << "CreateCapabilityMap called for device "
                   << deviceUniqueIdUTF8;

  wchar_t deviceIdW[kVideoCaptureUniqueNameLength];
  int deviceIdWLength = MultiByteToWideChar(CP_UTF8, 0, deviceUniqueIdUTF8, -1,
                                            deviceIdW, sizeof(deviceIdW));
  if (deviceIdWLength == 0) {
    RTC_LOG(LS_INFO) << "Failed to convert Device ID from UTF8, error = "
                     << GetLastError();
    return -1;
  }

  auto deviceId{to_hstring(deviceIdW)};

  auto videoCaptureDeviceAsync = DeviceInformation::CreateFromIdAsync(deviceId);
  blocking_suspend(videoCaptureDeviceAsync);
  if (videoCaptureDeviceAsync.Status() == AsyncStatus::Error) {
    return -1;
  }

  auto videoCaptureDevice = videoCaptureDeviceAsync.GetResults();

  MediaCaptureInitializationSettings settings;
  settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
  settings.StreamingCaptureMode(StreamingCaptureMode::Video);
  settings.VideoDeviceId(videoCaptureDevice.Id());

  MediaCapture mediaCapture;

  auto mediaCaptureInitializeAsync = mediaCapture.InitializeAsync(settings);
  blocking_suspend(mediaCaptureInitializeAsync);
  if (mediaCaptureInitializeAsync.Status() == AsyncStatus::Error) {
    return -1;
  }

  auto streamCapabilities =
      mediaCapture.VideoDeviceController().GetAvailableMediaStreamProperties(
          MediaStreamType::VideoRecord);

  VideoCaptureCapabilities videoCaptureCapabilities;
  for (auto streamCapability : streamCapabilities) {
    auto videoEncodingProperties =
        streamCapability.as<VideoEncodingProperties>();

    VideoCaptureCapability videoCaptureCapability;
    videoCaptureCapability.width = videoEncodingProperties.Width();
    videoCaptureCapability.height = videoEncodingProperties.Height();
    videoCaptureCapability.maxFPS =
        SafelyComputeMediaRatio(videoEncodingProperties.FrameRate());
    videoCaptureCapability.videoType =
        ToVideoType(videoEncodingProperties.Subtype());
    videoCaptureCapability.interlaced = false;

    videoCaptureCapabilities.push_back(videoCaptureCapability);
  }

  mediaCapture.Close();

  // Store the new used device name
  _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
  _lastUsedDeviceName =
      (char*)realloc(_lastUsedDeviceName, _lastUsedDeviceNameLength + 1);
  memcpy(_lastUsedDeviceName, deviceUniqueIdUTF8,
         _lastUsedDeviceNameLength + 1);

  _captureCapabilities = videoCaptureCapabilities;
  RTC_LOG(LS_INFO) << "CreateCapabilityMap " << _captureCapabilities.size();

  return static_cast<int32_t>(_captureCapabilities.size());
}

}  // namespace videocapturemodule
}  // namespace webrtc
