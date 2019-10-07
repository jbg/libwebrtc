/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/full_screen_window_detector_win.h"
#include <memory>
#include "modules/desktop_capture/win/full_screen_win_application_handler.h"

namespace webrtc {

void FullScreenWindowDetectorWin::CreateApplicationHandlerIfNeeded(
    DesktopCapturer::SourceId source_id) {
  if (app_handler_ == nullptr || app_handler_->GetSourceId() != source_id) {
    app_handler_ = CreateFullScreenWinApplicationHandler(source_id);
  }
}

rtc::scoped_refptr<FullScreenWindowDetector>
FullScreenWindowDetector::CreateFullScreenWindowDetector() {
  return rtc::scoped_refptr<FullScreenWindowDetector>(
      new FullScreenWindowDetectorWin());
}

}  // namespace webrtc
