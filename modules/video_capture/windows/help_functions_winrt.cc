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

uint32_t SafelyComputeMediaRatio(IMediaRatio* ratio_no_ref) {
  HRESULT hr;
  uint32_t numerator, denominator, media_ratio = 0;

  THR(ratio_no_ref->get_Numerator(&numerator));
  THR(ratio_no_ref->get_Denominator(&denominator));

  media_ratio = (denominator != 0) ? numerator / denominator : 0;

Cleanup:
  return media_ratio;
}

VideoType ToVideoType(const HString& sub_type) {
  uint32_t cchCount;
  const wchar_t* video_type = sub_type.GetRawBuffer(&cchCount);
  VideoType converted_type = VideoType::kUnknown;

  if (cchCount < 4 || cchCount > 8) {
    return VideoType::kUnknown;
  }

  struct {
    const wchar_t* format;
    const VideoType type;
  } static format_to_type[] = {
      {L"I420", VideoType::kI420},         {L"I420", VideoType::kI420},
      {L"IYUV", VideoType::kIYUV},         {L"RGB24", VideoType::kRGB24},
      {L"ABGR", VideoType::kABGR},         {L"ARGB", VideoType::kARGB},
      {L"ARGB4444", VideoType::kARGB4444}, {L"RGB565", VideoType::kRGB565},
      {L"RGB565", VideoType::kRGB565},     {L"ARGB1555", VideoType::kARGB1555},
      {L"YUY2", VideoType::kYUY2},         {L"YV12", VideoType::kYV12},
      {L"UYVY", VideoType::kUYVY},         {L"MJPEG", VideoType::kMJPEG},
      {L"NV21", VideoType::kNV21},         {L"NV12", VideoType::kNV12},
      {L"BGRA", VideoType::kBGRA},
  };

  for (const auto& entry : format_to_type) {
    if (wcsncmp(entry.format, video_type, cchCount) == 0) {
      converted_type = entry.type;
      break;
    }
  }

  return converted_type;
}

HRESULT WaitForAsyncAction(ComPtr<IAsyncAction> async_action) {
  HRESULT hr, hr_async_error;
  const DWORD timeout_ms = 2000;
  ComPtr<IAsyncInfo> async_info;
  Event event_completed;

#if _DEBUG
  APTTYPE apt_type;
  APTTYPEQUALIFIER apt_qualifier;
  THR(CoGetApartmentType(&apt_type, &apt_qualifier));
  // Please make the caller of this API run on a MTA appartment type.
  // The caller shouldn't be running on the UI thread (STA).
  assert(apt_type == APTTYPE_MTA);
#endif  // _DEBUG

  // Creates the Event to be used to block and suspend until the async
  // operation finishes.
  event_completed =
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
