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

using ABI::Windows::Foundation::IAsyncAction;
using ABI::Windows::Foundation::IAsyncActionCompletedHandler;
using ABI::Windows::Media::MediaProperties::IMediaRatio;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::Event;
using Microsoft::WRL::Wrappers::HString;

namespace webrtc {
namespace videocapturemodule {

uint32_t SafelyComputeMediaRatio(IMediaRatio* ratioNoRef) {
  uint32_t numerator, denominator;
  ratioNoRef->get_Denominator(&denominator);
  if (denominator == 0) {
    return 0;
  }
  ratioNoRef->get_Numerator(&numerator);

  return numerator / denominator;
}

VideoType ToVideoType(const HString& subtype) {
  uint32_t cchCount;
  const wchar_t* video_type = subtype.GetRawBuffer(&cchCount);

  if (cchCount < 4 || cchCount > 8) {
    return VideoType::kUnknown;
  }

  if (!wcsncmp(L"I420", video_type, cchCount))
    return VideoType::kI420;
  if (!wcsncmp(L"IYUV", video_type, cchCount))
    return VideoType::kIYUV;
  if (!wcsncmp(L"RGB24", video_type, cchCount))
    return VideoType::kRGB24;
  if (!wcsncmp(L"ABGR", video_type, cchCount))
    return VideoType::kABGR;
  if (!wcsncmp(L"ARGB", video_type, cchCount))
    return VideoType::kARGB;
  if (!wcsncmp(L"ARGB4444", video_type, cchCount))
    return VideoType::kARGB4444;
  if (!wcsncmp(L"RGB565", video_type, cchCount))
    return VideoType::kRGB565;
  if (!wcsncmp(L"RGB565", video_type, cchCount))
    return VideoType::kRGB565;
  if (!wcsncmp(L"ARGB1555", video_type, cchCount))
    return VideoType::kARGB1555;
  if (!wcsncmp(L"YUY2", video_type, cchCount))
    return VideoType::kYUY2;
  if (!wcsncmp(L"YV12", video_type, cchCount))
    return VideoType::kYV12;
  if (!wcsncmp(L"UYVY", video_type, cchCount))
    return VideoType::kUYVY;
  if (!wcsncmp(L"MJPEG", video_type, cchCount))
    return VideoType::kMJPEG;
  if (!wcsncmp(L"NV21", video_type, cchCount))
    return VideoType::kNV21;
  if (!wcsncmp(L"NV12", video_type, cchCount))
    return VideoType::kNV12;
  if (!wcsncmp(L"BGRA", video_type, cchCount))
    return VideoType::kBGRA;

  return VideoType::kUnknown;
}

HRESULT WaitForAsyncAction(ComPtr<IAsyncAction> async_action) {
  HRESULT hr, hr_async_error;
  const DWORD timeout_ms = 2000;
  ComPtr<IAsyncInfo> async_info;

  // Creates the Event to be used to block and suspend until the async
  // operation finishes.
  Event event_completed =
      Event(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET,
                          WRITE_OWNER | EVENT_ALL_ACCESS));
  THR(event_completed.IsValid() ? S_OK : E_HANDLE);

  // Defines the callback that will signal the event to unblock and resume.
  THR(async_action->put_Completed(
      Callback<IAsyncActionCompletedHandler>([&event_completed](
                                                 IAsyncAction*,
                                                 AsyncStatus async_status)
                                                 -> HRESULT {

        HRESULT hr;

        THR(async_status == AsyncStatus::Completed ? S_OK : E_ABORT);

      Cleanup:
        ::SetEvent(event_completed.Get());

        return hr;
      }).Get()));

  // Block and suspend thread until the async operation finishes or timeout.
  THR(::WaitForSingleObjectEx(event_completed.Get(), timeout_ms, FALSE) ==
              WAIT_OBJECT_0
          ? S_OK
          : E_FAIL);

  // Checks if async operation completed successfully.
  THR(async_action.As<IAsyncInfo>(&async_info));
  THR(async_info->get_ErrorCode(&hr_async_error));
  THR(hr_async_error);

Cleanup:
  return hr;
}

}  // namespace videocapturemodule
}  // namespace webrtc
