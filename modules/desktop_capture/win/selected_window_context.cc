/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/selected_window_context.h"

namespace webrtc {

SelectedWindowContext::SelectedWindowContext(
    HWND selected_window,
    DesktopRect selected_window_rect,
    WindowCaptureHelperWin* window_capture_helper)
    : selected_window(selected_window),
      window_capture_helper(window_capture_helper),
      selected_window_rect(selected_window_rect) {
  selected_window_thread_id =
      GetWindowThreadProcessId(selected_window, &selected_window_process_id);
}

bool SelectedWindowContext::IsSelectedWindowValid() const {
  return selected_window_thread_id != 0;
}

bool SelectedWindowContext::IsWindowOwned(HWND hwnd) const {
  // This check works for drop-down menus & dialog pop-up windows. It doesn't
  // work for context menus or tooltips, which are handled differently below.
  if (GetAncestor(hwnd, GA_ROOTOWNER) == selected_window) {
    return true;
  }

  // Some pop-up windows aren't owned (e.g. context menus, tooltips); treat
  // windows that belong to the same thread as owned.
  DWORD enumerated_window_process_id = 0;
  DWORD enumerated_window_thread_id =
      GetWindowThreadProcessId(hwnd, &enumerated_window_process_id);
  return enumerated_window_thread_id != 0 &&
         enumerated_window_process_id == selected_window_process_id &&
         enumerated_window_thread_id == selected_window_thread_id;
}

bool SelectedWindowContext::IsWindowOverlapping(HWND hwnd) const {
  return window_capture_helper->IsWindowIntersectWithSelectedWindow(
      hwnd, selected_window, selected_window_rect);
}

}  // namespace webrtc
