/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_WINDOWS_HELP_FUNCTIONS_WINRT_H_
#define MODULES_VIDEO_CAPTURE_WINDOWS_HELP_FUNCTIONS_WINRT_H_

#include <windows.foundation.h>
#include <windows.media.mediaproperties.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

#include "modules/video_capture/video_capture_defines.h"

// Evaluates an expression returning a HRESULT.
// It jumps to a label named Cleanup on failure.
#define THR(expr)   \
  do {              \
    hr = (expr);    \
    if (FAILED(hr)) \
      goto Cleanup; \
  } while (false)

namespace webrtc {
namespace videocapturemodule {

uint32_t SafelyComputeMediaRatio(
    ABI::Windows::Media::MediaProperties::IMediaRatio* ratioNoRef);

VideoType ToVideoType(const Microsoft::WRL::Wrappers::HString& sub_type);

HRESULT WaitForAsyncAction(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncAction>
        async_action);

template <typename T>
HRESULT WaitForAsyncOperation(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<T>>&
        async_op) {
  HRESULT hr, hr_async_error;
  const DWORD timeout_ms = 2000;
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncInfo> async_info;

  // Creates the Event to be used to block and suspend until the async
  // operation finishes.
  Microsoft::WRL::Wrappers::Event event_completed =
      Microsoft::WRL::Wrappers::Event(
          ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET,
                          WRITE_OWNER | EVENT_ALL_ACCESS));
  THR(event_completed.IsValid() ? S_OK : E_HANDLE);

  // Defines the callback that will signal the event to unblock and resume.
  THR(async_op->put_Completed(
      Microsoft::WRL::Callback<
          ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T>>(
          [&event_completed](
              ABI::Windows::Foundation::IAsyncOperation<T>*,
              ABI::Windows::Foundation::AsyncStatus async_status) -> HRESULT {
            HRESULT hr;

            THR(async_status == ABI::Windows::Foundation::AsyncStatus::Completed
                    ? S_OK
                    : E_ABORT);

          Cleanup:
            ::SetEvent(event_completed.Get());

            return hr;
          })
          .Get()));

  // Block and suspend thread until the async operation finishes or timeout.
  THR(::WaitForSingleObjectEx(event_completed.Get(), timeout_ms, FALSE) ==
              WAIT_OBJECT_0
          ? S_OK
          : E_FAIL);

  // Checks if async operation completed successfully.
  THR(async_op.template As<ABI::Windows::Foundation::IAsyncInfo>(&async_info));
  THR(async_info->get_ErrorCode(&hr_async_error));
  THR(hr_async_error);

Cleanup:
  return hr;
}

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_WINDOWS_HELP_FUNCTIONS_WINRT_H_
