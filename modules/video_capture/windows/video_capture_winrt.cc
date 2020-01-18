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

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Capture.Frames.h>
#include <winrt/Windows.Media.Capture.h>
#include <winrt/Windows.Media.MediaProperties.h>

#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/windows/device_info_winrt.h"
#include "modules/video_capture/windows/help_functions_winrt.h"
#include "rtc_base/logging.h"

struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(
    novtable) IMemoryBufferByteAccess : ::IUnknown {
  virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Capture;
using namespace Windows::Media::Capture::Frames;
using namespace Windows::Media::MediaProperties;

namespace webrtc {
namespace videocapturemodule {

///////////////////////////////////////////////////////////////////////////////
//
//  VideoCaptureWinRTInternal
//
///////////////////////////////////////////////////////////////////////////////

struct VideoCaptureWinRTInternal {
 public:
  VideoCaptureWinRTInternal(VideoCaptureImpl* pVideoCaptureImplNoRef)
      : video_capture_impl(pVideoCaptureImplNoRef) {}

  ~VideoCaptureWinRTInternal() {
    if (media_capture_ != nullptr) {
      StopCapture();
      media_capture_.Close();
    }
  }

  int32_t InitCamera(hstring& deviceId) {
    // Defines the settings to be used the camera
    auto settings = MediaCaptureInitializationSettings();
    settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    settings.VideoDeviceId(deviceId);

    if (media_capture_ != nullptr) {
      StopCapture();
      media_capture_.Close();
    }
    media_capture_ = MediaCapture();

    auto mediaCaptureInitializeAsync = media_capture_.InitializeAsync(settings);
    blocking_suspend(mediaCaptureInitializeAsync);
    if (mediaCaptureInitializeAsync.Status() == AsyncStatus::Error) {
      return -1;
    }

    return 0;
  }

  int32_t StartCapture(const VideoCaptureCapability& capability) {
    MediaFrameSource videoFrameSource = nullptr;
    for (auto kv : media_capture_.FrameSources()) {
      MediaFrameSource value = kv.Value();

      MediaFrameSourceInfo info = value.Info();
      if ((info.MediaStreamType() != MediaStreamType::VideoRecord) ||
          (info.SourceKind() != MediaFrameSourceKind::Color)) {
        continue;
      }

      for (auto format : value.SupportedFormats()) {
        VideoType subtype = ToVideoType(format.Subtype());
        if (subtype != capability.videoType && subtype != VideoType::kI420 &&
            subtype != VideoType::kYUY2 && subtype != VideoType::kYV12) {
          continue;
        }

        VideoMediaFrameFormat videoFormat = format.VideoFormat();
        if ((videoFormat.Width() != capability.width) ||
            (videoFormat.Height() != capability.height)) {
          continue;
        }

        MediaRatio frameRate = format.FrameRate();
        if (SafelyComputeMediaRatio(frameRate) >
            static_cast<uint32_t>(capability.maxFPS)) {
          continue;
        }

        videoFrameSource = value;

        auto setFormatAsync = videoFrameSource.SetFormatAsync(format);
        blocking_suspend(setFormatAsync);
        if (setFormatAsync.Status() == AsyncStatus::Error) {
          return -1;
        }

        break;
      }

      // The same camera might provide many sources, for example, Surface
      // Studio 2 camera has a color source provider and a depth source
      // provider. We don't need to continue looking for sources once the first
      // color source provider matches with the configuration we're looking for.
      if (videoFrameSource != nullptr) {
        break;
      }
    }

    // video capture device with capabilities not found
    if (videoFrameSource == nullptr) {
      return -1;
    }

    auto mediaCaptureCreateFrameReaderAsync =
        media_capture_.CreateFrameReaderAsync(videoFrameSource);
    blocking_suspend(mediaCaptureCreateFrameReaderAsync);
    if (mediaCaptureCreateFrameReaderAsync.Status() == AsyncStatus::Error) {
      return -1;
    }

    media_frame_reader = mediaCaptureCreateFrameReaderAsync.GetResults();
    media_frame_reader.FrameArrived(
        {this, &VideoCaptureWinRTInternal::FrameArrived});

    auto mediaFrameReaderStartAsync = media_frame_reader.StartAsync();
    blocking_suspend(mediaFrameReaderStartAsync);
    if (mediaFrameReaderStartAsync.Status() == AsyncStatus::Error) {
      return -1;
    }

    MediaFrameReaderStartStatus status =
        mediaFrameReaderStartAsync.GetResults();
    if (status != MediaFrameReaderStartStatus::Success) {
      return -1;
    }

    is_capturing = true;

    return 0;
  }

  int32_t StopCapture() {
    if (media_frame_reader == nullptr) {
      return 0;
    }

    if (is_capturing) {
      auto mediaFrameReaderStopAsync = media_frame_reader.StopAsync();
      blocking_suspend(mediaFrameReaderStopAsync);
      if (mediaFrameReaderStopAsync.Status() == AsyncStatus::Error) {
        return -1;
      }
    }
    is_capturing = false;

    media_frame_reader = nullptr;
    return 0;
  }

  bool CaptureStarted() { return is_capturing; }

  void FrameArrived(MediaFrameReader const& sender,
                    MediaFrameArrivedEventArgs const& args) {
    MediaFrameReference mediaFrameReference = sender.TryAcquireLatestFrame();
    if (mediaFrameReference != nullptr) {
      {
        VideoMediaFrame videoMediaFrame = mediaFrameReference.VideoMediaFrame();

        VideoMediaFrameFormat videoFormat = videoMediaFrame.VideoFormat();

        MediaFrameFormat frameFormat = videoFormat.MediaFrameFormat();

        MediaRatio frameRate = frameFormat.FrameRate();

        VideoCaptureCapability frameInfo;
        frameInfo.width = videoFormat.Width();
        frameInfo.height = videoFormat.Height();
        frameInfo.videoType = ToVideoType(frameFormat.Subtype());
        frameInfo.maxFPS = SafelyComputeMediaRatio(frameRate);
        frameInfo.interlaced = false;

        SoftwareBitmap softwareBitmap = videoMediaFrame.SoftwareBitmap();
        {
          BitmapBuffer bitmapBuffer =
              softwareBitmap.LockBuffer(BitmapBufferAccessMode::Read);
          {
            IMemoryBufferReference memoryBufferReference =
                bitmapBuffer.CreateReference();
            {
              uint8_t* value;
              uint32_t capacity;

              auto memoryBufferByteAccess =
                  memoryBufferReference.as<IMemoryBufferByteAccess>();
              check_hresult(
                  memoryBufferByteAccess->GetBuffer(&value, &capacity));

              video_capture_impl->IncomingFrame(value, capacity, frameInfo);
            }
            memoryBufferReference.Close();
          }
          bitmapBuffer.Close();
        }
        softwareBitmap.Close();
      }
      mediaFrameReference.Close();
    }
  }

 private:
  MediaCapture media_capture_{nullptr};
  MediaFrameReader media_frame_reader{nullptr};

  bool is_capturing = false;
  VideoCaptureImpl* video_capture_impl;
};

///////////////////////////////////////////////////////////////////////////////
//
//   VideoCaptureWinRT
//
///////////////////////////////////////////////////////////////////////////////

VideoCaptureWinRT::VideoCaptureWinRT()
    : video_capture_internal_(new VideoCaptureWinRTInternal(this)) {}

VideoCaptureWinRT::~VideoCaptureWinRT() {
  delete video_capture_internal_;
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
  }

  // Store the device name
  // VideoCaptureImpl::~VideoCaptureImpl reclaims _deviceUniqueId
  _deviceUniqueId = new char[deviceIdLength + 1];
  memcpy(_deviceUniqueId, deviceUniqueIdUTF8, deviceIdLength + 1);

  return 0;
}

int32_t VideoCaptureWinRT::Init(const char* deviceUniqueIdUTF8) {
  // Gets hstring from deviceId utf8
  wchar_t deviceIdW[kVideoCaptureUniqueNameLength];
  int deviceIdWLength = MultiByteToWideChar(CP_UTF8, 0, deviceUniqueIdUTF8, -1,
                                            deviceIdW, sizeof(deviceIdW));
  if (deviceIdWLength == 0) {
    return -1;
  }
  auto videoDeviceId = to_hstring(deviceIdW);

  // Sets _deviceUniqueId defined by the super class
  if (SetDeviceUniqueId(deviceUniqueIdUTF8)) {
    return -1;
  }

  // Initializes the camera with desired settings
  if (video_capture_internal_->InitCamera(videoDeviceId)) {
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

  int32_t ret = video_capture_internal_->StartCapture(capability);

  if (ret == 0) {
    _requestedCapability = capability;
  }

  return ret;
}

int32_t VideoCaptureWinRT::StopCapture() {
  rtc::CritScope cs(&_apiCs);
  return video_capture_internal_->StopCapture();
}

bool VideoCaptureWinRT::CaptureStarted() {
  return video_capture_internal_->CaptureStarted();
}

int32_t VideoCaptureWinRT::CaptureSettings(VideoCaptureCapability& settings) {
  settings = _requestedCapability;
  return 0;
}

}  // namespace videocapturemodule
}  // namespace webrtc
