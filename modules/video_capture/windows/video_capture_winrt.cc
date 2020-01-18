/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/windows/video_capture_winrt.h"

#include <Windows.Devices.Enumeration.h>
#include <Windows.Foundation.Collections.h>
#include <unknwn.h>
#include <windows.foundation.h>
#include <windows.media.capture.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include <cassert>
#include <functional>

#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/windows/help_functions_winrt.h"
#include "rtc_base/logging.h"

struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(
    novtable) IMemoryBufferByteAccess : ::IUnknown {
  virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::Capture::Frames;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Graphics::Imaging;

namespace webrtc {
namespace videocapturemodule {

///////////////////////////////////////////////////////////////////////////////
//
//  VideoCaptureWinRTInternal
//
///////////////////////////////////////////////////////////////////////////////

// Callback type for the following method:
// int32_t VideoCaptureImpl::IncomingFrame(uint8_t* videoFrame,
//                     size_t videoFrameLength,
//                     const VideoCaptureCapability& frameInfo,
//                     int64_t captureTime = 0);
typedef std::function<
    int32_t(uint8_t*, size_t, const VideoCaptureCapability&, int64_t)>
    PFNIncomingFrameType;

struct VideoCaptureWinRTInternal {
 public:
  VideoCaptureWinRTInternal(const PFNIncomingFrameType& pfn_incoming_frame);

  ~VideoCaptureWinRTInternal();

  HRESULT InitCamera(const wchar_t* pDeviceId);
  HRESULT StartCapture(const VideoCaptureCapability& capability);
  HRESULT StopCapture();
  bool CaptureStarted();

  HRESULT FrameArrived(
      ABI::Windows::Media::Capture::Frames::IMediaFrameReader* sender,
      ABI::Windows::Media::Capture::Frames::IMediaFrameArrivedEventArgs* args);

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCapture>
      media_capture_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::Capture::Frames::IMediaFrameReader>
      media_frame_reader_;
  EventRegistrationToken media_source_frame_arrived_token;

  bool is_capturing = false;
  PFNIncomingFrameType pfn_incoming_frame_;
};

VideoCaptureWinRTInternal::VideoCaptureWinRTInternal(
    const PFNIncomingFrameType& pfn_incoming_frame)
    : pfn_incoming_frame_(pfn_incoming_frame) {}

VideoCaptureWinRTInternal::~VideoCaptureWinRTInternal() {
  HRESULT hr;

  if (media_capture_) {
    ComPtr<IClosable> closable;

    THR(StopCapture());
    THR(media_capture_.As(&closable));
    THR(closable->Close());
  }

Cleanup:
  assert(SUCCEEDED(hr));
}

HRESULT VideoCaptureWinRTInternal::InitCamera(const wchar_t* device_unique_id) {
  HRESULT hr;
  ComPtr<IMediaCaptureInitializationSettings> settings;
  ComPtr<IMediaCaptureInitializationSettings5> settings5;
  ComPtr<IAsyncAction> async_action;
  HStringReference device_id(device_unique_id);

  if (media_capture_) {
    ComPtr<IClosable> closable_media_capture;
    THR(media_capture_.As(&closable_media_capture));

    THR(StopCapture() == 0 ? S_OK : E_FAIL);
    THR(closable_media_capture->Close());
  }
  THR(ActivateInstance(
      HStringReference(RuntimeClass_Windows_Media_Capture_MediaCapture).Get(),
      media_capture_.ReleaseAndGetAddressOf()));

  // Defines the settings to be used the camera
  THR(ActivateInstance(
      HStringReference(
          RuntimeClass_Windows_Media_Capture_MediaCaptureInitializationSettings)
          .Get(),
      &settings));
  THR(settings.As(&settings5));
  THR(settings5->put_MemoryPreference(
      MediaCaptureMemoryPreference::MediaCaptureMemoryPreference_Cpu));
  THR(settings->put_StreamingCaptureMode(
      StreamingCaptureMode::StreamingCaptureMode_Video));
  THR(settings->put_VideoDeviceId(device_id.Get()));

  THR(media_capture_->InitializeWithSettingsAsync(settings.Get(),
                                                  &async_action));
  THR(WaitForAsyncAction(async_action));

Cleanup:
  return hr;
}

HRESULT VideoCaptureWinRTInternal::StartCapture(
    const VideoCaptureCapability& capability) {
  HRESULT hr;
  ComPtr<IMediaFrameSource> media_frame_source;
  ComPtr<IMediaCapture5> media_capture5;
  ComPtr<IMapView<HSTRING, MediaFrameSource*>> frame_sources;
  ComPtr<IIterable<IKeyValuePair<HSTRING, MediaFrameSource*>*>> iterable;
  ComPtr<IIterator<IKeyValuePair<HSTRING, MediaFrameSource*>*>> iterator;
  ComPtr<IAsyncOperation<MediaFrameReader*>> async_operation;
  ComPtr<IAsyncOperation<MediaFrameReaderStartStatus>>
      async_media_frame_reader_start_status;
  MediaFrameReaderStartStatus media_frame_reader_start_status;
  boolean has_current;

  THR(media_capture_.As(&media_capture5));
  THR(media_capture5->get_FrameSources(&frame_sources));
  THR(frame_sources.As(&iterable));
  THR(iterable->First(&iterator));
  THR(iterator->get_HasCurrent(&has_current));

  while (has_current) {
    ComPtr<IKeyValuePair<HSTRING, MediaFrameSource*>> key_value;
    ComPtr<IMediaFrameSource> value;
    ComPtr<IMediaFrameSourceInfo> info;
    ComPtr<IVectorView<MediaFrameFormat*>> supported_formats;
    ComPtr<IVideoMediaFrameFormat> video_media_frame_format;
    ComPtr<IMediaRatio> media_ratio;
    MediaStreamType media_stream_type;
    MediaFrameSourceKind media_frame_source_kind;
    unsigned int supported_formats_count;
    UINT32 frame_width, frame_height;

    THR(iterator->get_Current(&key_value));
    THR(key_value->get_Value(&value));

    // Filters out frame sources other than color video cameras.
    THR(value->get_Info(&info));
    THR(info->get_MediaStreamType(&media_stream_type));
    THR(info->get_SourceKind(&media_frame_source_kind));
    if ((media_stream_type != MediaStreamType::MediaStreamType_VideoRecord) ||
        (media_frame_source_kind !=
         MediaFrameSourceKind::MediaFrameSourceKind_Color)) {
      THR(iterator->MoveNext(&has_current));
      continue;
    }

    THR(value->get_SupportedFormats(&supported_formats));
    THR(supported_formats->get_Size(&supported_formats_count));

    for (unsigned int i = 0; i < supported_formats_count; ++i) {
      ComPtr<IMediaFrameFormat> media_frame_format;
      ComPtr<IAsyncAction> async_action;
      HString sub_type;

      THR(supported_formats->GetAt(i, &media_frame_format));

      // Filters out frame sources not sending frames in I420, YUY2, YV12 or
      // the format requested by the requested capabilities.
      THR(media_frame_format->get_Subtype(sub_type.ReleaseAndGetAddressOf()));
      VideoType video_type = ToVideoType(sub_type);
      if (video_type != capability.videoType &&
          video_type != VideoType::kI420 && video_type != VideoType::kYUY2 &&
          video_type != VideoType::kYV12) {
        THR(iterator->MoveNext(&has_current));
        continue;
      }

      // Filters out frame sources with resolution different than the one
      // defined by the requested capabilities.
      THR(media_frame_format->get_VideoFormat(&video_media_frame_format));
      THR(video_media_frame_format->get_Width(&frame_width));
      THR(video_media_frame_format->get_Height(&frame_height));
      if ((frame_width != capability.width) ||
          (frame_height != capability.height)) {
        THR(iterator->MoveNext(&has_current));
        continue;
      }

      // Filters os frames sources with frame rate higher than the what is
      // requested by the capabilities.
      THR(media_frame_format->get_FrameRate(&media_ratio));
      if (SafelyComputeMediaRatio(media_ratio.Get()) >
          static_cast<uint32_t>(capability.maxFPS)) {
        THR(iterator->MoveNext(&has_current));
        continue;
      }

      // Select this as media frame source.
      media_frame_source = value;

      THR(media_frame_source->SetFormatAsync(media_frame_format.Get(),
                                             &async_action));
      THR(WaitForAsyncAction(async_action));

      break;
    }

    // The same camera might provide many sources, for example, Surface
    // Studio 2 camera has a color source provider and a depth source
    // provider. We don't need to continue looking for sources once the first
    // color source provider matches with the configuration we're looking for.
    if (media_frame_source) {
      break;
    }
  }

  // video capture device with capabilities not found
  THR(media_frame_source ? S_OK : E_FAIL);

  THR(media_capture5->CreateFrameReaderAsync(media_frame_source.Get(),
                                             &async_operation));
  THR(WaitForAsyncOperation(async_operation));

  // Assigns a new media frame reader
  THR(async_operation->GetResults(&media_frame_reader_));

  THR(media_frame_reader_->add_FrameArrived(
      Callback<
          ITypedEventHandler<MediaFrameReader*, MediaFrameArrivedEventArgs*>>(
          [this](IMediaFrameReader* pSender,
                 IMediaFrameArrivedEventArgs* pEventArgs) {
            return this->FrameArrived(pSender, pEventArgs);
          })
          .Get(),
      &media_source_frame_arrived_token));

  THR(media_frame_reader_->StartAsync(&async_media_frame_reader_start_status));
  THR(WaitForAsyncOperation(async_media_frame_reader_start_status));

  THR(async_media_frame_reader_start_status->GetResults(
      &media_frame_reader_start_status));
  THR(media_frame_reader_start_status !=
              MediaFrameReaderStartStatus::MediaFrameReaderStartStatus_Success
          ? E_FAIL
          : S_OK);

  is_capturing = true;

Cleanup:
  return hr;
}

HRESULT VideoCaptureWinRTInternal::StopCapture() {
  HRESULT hr;

  if (!media_frame_reader_) {
    return S_OK;
  }

  if (is_capturing) {
    ComPtr<IAsyncAction> async_action;

    THR(media_frame_reader_->remove_FrameArrived(
        media_source_frame_arrived_token));
    THR(media_frame_reader_->StopAsync(&async_action));
    THR(WaitForAsyncAction(async_action));
  }

  is_capturing = false;

Cleanup:
  media_frame_reader_.Reset();
  return hr;
}

bool VideoCaptureWinRTInternal::CaptureStarted() {
  return is_capturing;
}

HRESULT VideoCaptureWinRTInternal::FrameArrived(
    ABI::Windows::Media::Capture::Frames::IMediaFrameReader* sender_no_ref,
    ABI::Windows::Media::Capture::Frames::IMediaFrameArrivedEventArgs*
        args_no_ref) {
  UNREFERENCED_PARAMETER(args_no_ref);

  HRESULT hr;
  ComPtr<IMediaFrameReader> media_frame_reader{sender_no_ref};
  ComPtr<IMediaFrameReference> media_frame_reference;
  ComPtr<IVideoMediaFrame> video_media_frame;
  ComPtr<IVideoMediaFrameFormat> video_media_frame_format;
  ComPtr<IMediaFrameFormat> media_frame_format;
  ComPtr<IMediaRatio> media_ratio;
  ComPtr<ISoftwareBitmap> software_bitmap;
  ComPtr<IBitmapBuffer> bitmap_buffer;
  ComPtr<IMemoryBuffer> memory_buffer;
  ComPtr<IMemoryBufferReference> memory_buffer_reference;
  ComPtr<IMemoryBufferByteAccess> memory_buffer_byte_access;
  HString video_subtype;
  uint8_t* bitmap_content;
  uint32_t bitmap_capacity;

  THR(media_frame_reader->TryAcquireLatestFrame(&media_frame_reference));

  if (media_frame_reference) {
    THR(media_frame_reference->get_VideoMediaFrame(&video_media_frame));
    THR(video_media_frame->get_VideoFormat(&video_media_frame_format));
    THR(video_media_frame_format->get_MediaFrameFormat(&media_frame_format));
    THR(media_frame_format->get_FrameRate(&media_ratio));

    VideoCaptureCapability frameInfo;
    THR(video_media_frame_format->get_Width(
        reinterpret_cast<UINT32*>(&frameInfo.width)));
    THR(video_media_frame_format->get_Height(
        reinterpret_cast<UINT32*>(&frameInfo.height)));
    THR(media_frame_format->get_Subtype(
        video_subtype.ReleaseAndGetAddressOf()));
    frameInfo.videoType = ToVideoType(video_subtype);
    frameInfo.maxFPS = SafelyComputeMediaRatio(media_ratio.Get());
    frameInfo.interlaced = false;

    THR(video_media_frame->get_SoftwareBitmap(&software_bitmap));
    THR(software_bitmap->LockBuffer(
        BitmapBufferAccessMode::BitmapBufferAccessMode_Read, &bitmap_buffer));
    THR(bitmap_buffer.As(&memory_buffer));
    THR(memory_buffer->CreateReference(&memory_buffer_reference));
    THR(memory_buffer_reference.As(&memory_buffer_byte_access));
    THR(memory_buffer_byte_access->GetBuffer(&bitmap_content,
                                             &bitmap_capacity));

    pfn_incoming_frame_(bitmap_content, bitmap_capacity, frameInfo, 0);
  }

Cleanup:
  if (memory_buffer_reference) {
    ComPtr<IClosable> closable;
    memory_buffer_reference.As(&closable);
    closable->Close();
  }

  if (bitmap_buffer) {
    ComPtr<IClosable> closable;
    bitmap_buffer.As(&closable);
    closable->Close();
  }

  if (software_bitmap) {
    ComPtr<IClosable> closable;
    software_bitmap.As(&closable);
    closable->Close();
  }

  if (media_frame_reference) {
    ComPtr<IClosable> closable;
    media_frame_reference.As(&closable);
    closable->Close();
  }

  return hr;
}

// Avoids forward declaring VideoCaptureWinRTInternal in the header.
constexpr VideoCaptureWinRTInternal* Impl(void* video_capture_internal) {
  return static_cast<VideoCaptureWinRTInternal*>(video_capture_internal);
}

///////////////////////////////////////////////////////////////////////////////
//
//   VideoCaptureWinRT
//
///////////////////////////////////////////////////////////////////////////////

VideoCaptureWinRT::VideoCaptureWinRT()
    : video_capture_internal_(new VideoCaptureWinRTInternal(
          std::bind(&VideoCaptureWinRT::IncomingFrame,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3,
                    std::placeholders::_4))) {}

VideoCaptureWinRT::~VideoCaptureWinRT() {
  delete Impl(video_capture_internal_);
}

// Helper method for filling _deviceUniqueId defined by the super class
int32_t VideoCaptureWinRT::SetDeviceUniqueId(const char* deviceUniqueIdUTF8) {
  auto deviceIdLength =
      strnlen(deviceUniqueIdUTF8, kVideoCaptureUniqueNameLength);

  if (deviceIdLength == kVideoCaptureUniqueNameLength) {
    RTC_LOG(LS_INFO) << "deviceUniqueId too long";
    return -1;
  }

  if (_deviceUniqueId) {
    RTC_LOG(LS_INFO) << "_deviceUniqueId leaked";
    delete[] _deviceUniqueId;
    _deviceUniqueId = nullptr;
  }

  // Store the device name
  // VideoCaptureImpl::~VideoCaptureImpl reclaims _deviceUniqueId
  _deviceUniqueId = new char[deviceIdLength + 1];
  memcpy(_deviceUniqueId, deviceUniqueIdUTF8, deviceIdLength + 1);

  return 0;
}

int32_t VideoCaptureWinRT::Init(const char* device_unique_id_UTF8) {
  // Gets hstring from deviceId utf8
  wchar_t device_id_w[kVideoCaptureUniqueNameLength];
  int device_id_w_length = MultiByteToWideChar(
      CP_UTF8, 0, device_unique_id_UTF8, -1, device_id_w, sizeof(device_id_w));
  if (device_id_w_length == 0) {
    return -1;
  }

  // Sets _deviceUniqueId defined by the super class
  if (SetDeviceUniqueId(device_unique_id_UTF8)) {
    return -1;
  }

  // Initializes the camera with desired settings
  if (Impl(video_capture_internal_)->InitCamera(device_id_w)) {
    return -1;
  }

  return 0;
}

int32_t VideoCaptureWinRT::StartCapture(
    const VideoCaptureCapability& capability) {
  rtc::CritScope cs(&_apiCs);

  if (CaptureStarted()) {
    if (capability == _requestedCapability) {
      return 0;
    }
    StopCapture();
  }

  int32_t ret = Impl(video_capture_internal_)->StartCapture(capability);

  if (SUCCEEDED(ret)) {
    _requestedCapability = capability;
  }

  return ret;
}

int32_t VideoCaptureWinRT::StopCapture() {
  rtc::CritScope cs(&_apiCs);
  return Impl(video_capture_internal_)->StopCapture();
}

bool VideoCaptureWinRT::CaptureStarted() {
  return Impl(video_capture_internal_)->CaptureStarted();
}

int32_t VideoCaptureWinRT::CaptureSettings(VideoCaptureCapability& settings) {
  settings = _requestedCapability;
  return 0;
}

}  // namespace videocapturemodule
}  // namespace webrtc
