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

#include <Windows.Devices.Enumeration.h>
#include <Windows.Foundation.Collections.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/windows/help_functions_winrt.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"

using namespace std;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::Devices;
using namespace ABI::Windows::Media::MediaProperties;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace webrtc {
namespace videocapturemodule {

///////////////////////////////////////////////////////////////////////////////
//
// DeviceInfoWinRTInternal
//
///////////////////////////////////////////////////////////////////////////////
struct DeviceInfoWinRTInternal {
  DeviceInfoWinRTInternal();

  ~DeviceInfoWinRTInternal() = default;

  HRESULT Init();

  HRESULT GetNumberOfDevices(uint32_t* device_count);

  HRESULT GetDeviceName(uint32_t device_number,
                        char* device_name_UTF8,
                        uint32_t device_name_length,
                        char* device_unique_id_UTF8,
                        uint32_t device_unique_id_UTF8_length,
                        char* product_unique_id_UTF8,
                        uint32_t product_unique_id_UTF8_length);

  HRESULT CreateCapabilityMap(
      const wchar_t* device_unique_id,
      vector<VideoCaptureCapability>& video_capture_capabilities);

 private:
  HRESULT AssureInitialized();

  HRESULT DeviceInfoWinRTInternal::GetDeviceInformationCollection(
      ComPtr<IVectorView<DeviceInformation*>>& device_collection);

  RoInitializeWrapper ro_initialize_wrapper_;
  ComPtr<IDeviceInformationStatics> device_info_statics_;
};

DeviceInfoWinRTInternal::DeviceInfoWinRTInternal()
    : ro_initialize_wrapper_(RO_INIT_MULTITHREADED) {}

HRESULT DeviceInfoWinRTInternal::Init() {
  HRESULT hr;

  // Checks if Windows runtime initialized successfully.
  // Pay attention if ro_initialize_wrapper_ is returning RPC_E_CHANGED_MODE.
  // It means device_info_winrt is being called from an apartment thread (STA)
  // instead of multithreaded (MTA). Usually this means device_info_winrt is
  // being called from the UI thread.
  THR((HRESULT)ro_initialize_wrapper_);

  // Get the object containing the DeviceInformation static methods.
  THR(GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_Devices_Enumeration_DeviceInformation)
          .Get(),
      &device_info_statics_));

Cleanup:
  return hr;
}

HRESULT DeviceInfoWinRTInternal::AssureInitialized() {
  HRESULT hr = S_OK;

  // Some sample apps don't call DeviceInfoImpl::Init before using the class.
  // All public methods of this class should assure its initialization.
  if (FAILED(ro_initialize_wrapper_) || !device_info_statics_.Get()) {
    RTC_LOG(LS_INFO) << "DeviceInfoWinRT wasn't initialized. Initializing.";
    THR(Init());
  }

Cleanup:
  return hr;
}

HRESULT DeviceInfoWinRTInternal::GetDeviceInformationCollection(
    ComPtr<IVectorView<DeviceInformation*>>& device_collection) {
  HRESULT hr;
  ComPtr<IAsyncOperation<DeviceInformationCollection*>>
      async_op_device_info_collection;

  // Call FindAllAsync and then start the async operation.
  THR(device_info_statics_->FindAllAsyncDeviceClass(
      DeviceClass_VideoCapture, &async_op_device_info_collection));

  // Block and suspend thread until the async operation finishes or timeouts.
  THR(WaitForAsyncOperation(async_op_device_info_collection));

  // Returns device collection if async operation completed successfully.
  THR(async_op_device_info_collection->GetResults(&device_collection));

Cleanup:
  return hr;
}

HRESULT DeviceInfoWinRTInternal::GetNumberOfDevices(uint32_t* device_count) {
  HRESULT hr;
  ComPtr<IVectorView<DeviceInformation*>> device_info_collection;

  // Assures class has been properly initialized.
  THR(AssureInitialized());

  THR(GetDeviceInformationCollection(device_info_collection));
  THR(device_info_collection->get_Size(device_count));

Cleanup:
  return hr;
}

HRESULT DeviceInfoWinRTInternal::GetDeviceName(
    uint32_t device_number,
    char* device_name_UTF8,
    uint32_t device_name_length,
    char* device_unique_id_UTF8,
    uint32_t device_unique_id_UTF8_length,
    char* product_unique_id_UTF8,
    uint32_t product_unique_id_UTF8_length) {
  HRESULT hr;
  uint32_t device_count;
  ComPtr<IVectorView<DeviceInformation*>> device_info_collection;
  ComPtr<IDeviceInformation> device_info;

  // Assures class has been properly initialized.
  THR(AssureInitialized());

  // Gets the device information collection synchronously
  THR(GetDeviceInformationCollection(device_info_collection));

  // Checks if desired device index is within the collection
  THR(device_info_collection->get_Size(&device_count));
  if (device_number >= device_count) {
    RTC_LOG(LS_INFO) << "Device number is out of bounds";
    THR(E_BOUNDS);
  }

  THR(device_info_collection->GetAt(device_number, &device_info));

  if (device_name_length > 0) {
    HString video_capture_name;
    THR(device_info->get_Name(video_capture_name.ReleaseAndGetAddressOf()));

    // rtc::ToUtf8 does not check for errors
    if (::WideCharToMultiByte(
            CP_UTF8, 0, video_capture_name.GetRawBuffer(nullptr), -1,
            (char*)device_name_UTF8, device_name_length, NULL, NULL) == 0) {
      RTC_LOG(LS_INFO) << "Failed to convert device name to UTF8, error = "
                       << GetLastError();
      THR(E_FAIL);
    }
  }

  if (device_unique_id_UTF8_length > 0) {
    HString video_capture_id;
    THR(device_info->get_Id(video_capture_id.ReleaseAndGetAddressOf()));

    // rtc::ToUtf8 does not check for errors
    if (::WideCharToMultiByte(CP_UTF8, 0,
                              video_capture_id.GetRawBuffer(nullptr), -1,
                              (char*)device_unique_id_UTF8,
                              device_unique_id_UTF8_length, NULL, NULL) == 0) {
      RTC_LOG(LS_INFO) << "Failed to convert device id to UTF8, error = "
                       << GetLastError();
      THR(E_FAIL);
    }
  }

  if (product_unique_id_UTF8_length > 0) {
    HString video_capture_id;
    THR(device_info->get_Id(video_capture_id.ReleaseAndGetAddressOf()));

    auto wsDeviceId = wstringstream(video_capture_id.GetRawBuffer(nullptr));

    wstring wsProductUniqueId;
    if (!getline(wsDeviceId, wsProductUniqueId, L'&') ||
        !getline(wsDeviceId, wsProductUniqueId, L'&')) {
      RTC_LOG(LS_INFO) << "Failed to parse product unique id";
      THR(E_FAIL);
    }

    // rtc::ToUtf8 does not check for errors
    if (::WideCharToMultiByte(CP_UTF8, 0, wsProductUniqueId.c_str(), -1,
                              (char*)product_unique_id_UTF8,
                              product_unique_id_UTF8_length, NULL, NULL) == 0) {
      RTC_LOG(LS_INFO)
          << "Failed to convert product unique id to UTF8, error = "
          << GetLastError();
      THR(E_FAIL);
    }
  }

Cleanup:
  return hr;
}

HRESULT DeviceInfoWinRTInternal::CreateCapabilityMap(
    const wchar_t* device_unique_id,
    vector<VideoCaptureCapability>& video_capture_capabilities) {
  HRESULT hr;
  ComPtr<IAsyncOperation<DeviceInformation*>> async_op_device_information;
  ComPtr<IDeviceInformation> device_info;
  ComPtr<IMediaCaptureInitializationSettings> init_settings;
  ComPtr<IMediaCaptureInitializationSettings5> init_settings5;
  ComPtr<IMediaCapture> media_capture;
  ComPtr<IClosable> media_capture_closable;
  ComPtr<IAsyncAction> async_action;
  ComPtr<IVideoDeviceController> video_device_controller;
  ComPtr<IMediaDeviceController> media_device_controller;
  ComPtr<IVectorView<IMediaEncodingProperties*>> stream_capabilities;
  HStringReference device_id(device_unique_id);
  unsigned int stream_capabilities_size;
  vector<VideoCaptureCapability> device_caps;

  // Assures class has been properly initialized.
  THR(AssureInitialized());

  // Calls FindAllAsync and then start the async operation.
  THR(device_info_statics_->CreateFromIdAsync(device_id.Get(),
                                              &async_op_device_information));

  // Blocks and suspends thread until the async operation finishes or
  // timeouts. Timeout is taked as failure.
  THR(WaitForAsyncOperation(async_op_device_information));

  // Gathers device information.
  THR(async_op_device_information->GetResults(&device_info));

  //
  THR(ActivateInstance(
      HStringReference(
          RuntimeClass_Windows_Media_Capture_MediaCaptureInitializationSettings)
          .Get(),
      &init_settings));
  THR(init_settings.As<IMediaCaptureInitializationSettings5>(&init_settings5));
  THR(init_settings5->put_MemoryPreference(
      MediaCaptureMemoryPreference::MediaCaptureMemoryPreference_Cpu));
  THR(init_settings->put_StreamingCaptureMode(
      StreamingCaptureMode::StreamingCaptureMode_Video));
  THR(init_settings->put_VideoDeviceId(device_id.Get()));

  THR(ActivateInstance(
      HStringReference(RuntimeClass_Windows_Media_Capture_MediaCapture).Get(),
      &media_capture));

  THR(media_capture->InitializeWithSettingsAsync(init_settings.Get(),
                                                 &async_action));
  THR(WaitForAsyncAction(async_action));

  THR(media_capture->get_VideoDeviceController(&video_device_controller));
  THR(video_device_controller.As<IMediaDeviceController>(
      &media_device_controller));

  THR(media_device_controller->GetAvailableMediaStreamProperties(
      MediaStreamType::MediaStreamType_VideoRecord, &stream_capabilities));

  THR(stream_capabilities->get_Size(&stream_capabilities_size));
  for (unsigned int i = 0; i < stream_capabilities_size; ++i) {
    ComPtr<IMediaEncodingProperties> media_encoding_props;
    ComPtr<IVideoEncodingProperties> video_encoding_props;
    ComPtr<IMediaRatio> media_ratio;
    VideoCaptureCapability video_capture_capability;
    HString subtype;

    THR(stream_capabilities->GetAt(i, &media_encoding_props));

    THR(media_encoding_props.As<IVideoEncodingProperties>(
        &video_encoding_props));

    THR(video_encoding_props->get_Width(
        reinterpret_cast<UINT32*>(&video_capture_capability.width)));

    THR(video_encoding_props->get_Height(
        reinterpret_cast<UINT32*>(&video_capture_capability.height)));

    THR(video_encoding_props->get_FrameRate(&media_ratio));
    video_capture_capability.maxFPS =
        SafelyComputeMediaRatio(media_ratio.Get());

    THR(media_encoding_props->get_Subtype(subtype.ReleaseAndGetAddressOf()));
    video_capture_capability.videoType = ToVideoType(subtype);

    video_capture_capability.interlaced = false;

    device_caps.push_back(video_capture_capability);
  }

  THR(media_capture.As<IClosable>(&media_capture_closable));
  THR(media_capture_closable->Close());

  video_capture_capabilities.swap(device_caps);

Cleanup:
  return hr;
}

// Allows not forward declaring DeviceInfoWinRTInternal in the header.
constexpr DeviceInfoWinRTInternal* Impl(void* device_info_internal) {
  return static_cast<DeviceInfoWinRTInternal*>(device_info_internal);
}

///////////////////////////////////////////////////////////////////////////////
//
// DeviceInfoWinRT
//
///////////////////////////////////////////////////////////////////////////////

// static
DeviceInfoWinRT* DeviceInfoWinRT::Create() {
  return new DeviceInfoWinRT();
}

DeviceInfoWinRT::DeviceInfoWinRT()
    : device_info_internal_(new DeviceInfoWinRTInternal()) {}

DeviceInfoWinRT::~DeviceInfoWinRT() {
  delete Impl(device_info_internal_);
}

int32_t DeviceInfoWinRT::Init() {
  return SUCCEEDED(Impl(device_info_internal_)->Init()) ? 0 : -1;
}

uint32_t DeviceInfoWinRT::NumberOfDevices() {
  ReadLockScoped cs(_apiLock);

  HRESULT hr;
  uint32_t device_count = -1;

  THR(Impl(device_info_internal_)->GetNumberOfDevices(&device_count));

Cleanup:
  return SUCCEEDED(hr) ? device_count : -1;
}

int32_t DeviceInfoWinRT::GetDeviceName(uint32_t device_number,
                                       char* device_name_UTF8,
                                       uint32_t device_name_length,
                                       char* device_unique_id_UTF8,
                                       uint32_t device_unique_id_UTF8_length,
                                       char* product_unique_id_UTF8,
                                       uint32_t product_unique_id_UTF8_length) {
  ReadLockScoped cs(_apiLock);

  HRESULT hr;

  THR(Impl(device_info_internal_)
          ->GetDeviceName(device_number, device_name_UTF8, device_name_length,
                          device_unique_id_UTF8, device_unique_id_UTF8_length,
                          product_unique_id_UTF8,
                          product_unique_id_UTF8_length));

  if (device_name_length) {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " " << device_name_UTF8;
  }

Cleanup:
  return SUCCEEDED(hr) ? 0 : -1;
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
  int deviceIdWLength = ::MultiByteToWideChar(CP_UTF8, 0, deviceUniqueIdUTF8,
                                              -1, deviceIdW, sizeof(deviceIdW));
  if (deviceIdWLength == 0) {
    RTC_LOG(LS_INFO) << "Failed to convert Device ID from UTF8, error = "
                     << GetLastError();
    return -1;
  }

  if (FAILED(Impl(device_info_internal_)
                 ->CreateCapabilityMap(deviceIdW, _captureCapabilities))) {
    return -1;
  }

  // Store the new used device name
  _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
  _lastUsedDeviceName =
      (char*)realloc(_lastUsedDeviceName, _lastUsedDeviceNameLength + 1);
  memcpy(_lastUsedDeviceName, deviceUniqueIdUTF8,
         _lastUsedDeviceNameLength + 1);

  RTC_LOG(LS_INFO) << "CreateCapabilityMap " << _captureCapabilities.size();

  return static_cast<int32_t>(_captureCapabilities.size());
}

}  // namespace videocapturemodule
}  // namespace webrtc
