/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/desktop_capture/mac/full_screen_window_detector_mac.h"
#include "modules/desktop_capture/mac/full_screen_mac_application_handler.h"
#include "modules/desktop_capture/mac/window_list_utils.h"

namespace webrtc {

void FullScreenWindowDetectorMac::CreateApplicationHandlerIfNeeded(
    DesktopCapturer::SourceId source_id) {
  if (app_handler_ == nullptr || app_handler_->GetSourceId() != source_id) {
    app_handler_ = CreateFullScreenMacApplicationHandler(source_id);
  }
}

rtc::scoped_refptr<FullScreenWindowDetector>
    FullScreenWindowDetector::CreateFullScreenWindowDetector() {
  return rtc::scoped_refptr<FullScreenWindowDetector>(new FullScreenWindowDetectorMac());
}

}  // namespace webrtc
