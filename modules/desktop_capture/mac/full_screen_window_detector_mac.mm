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

bool FullScreenWindowDetectorMac::UpdateWindowList(
    DesktopCapturer::SourceId original_source_id,
    DesktopCapturer::SourceList* sources,
    rtc::FunctionView<bool(DesktopCapturer::SourceList*)> get_sources) {
  if (!IsWindowOnScreen(original_source_id)) {
    // No need to update the window list when the window is minimized.
    return false;
  }
  return get_sources(sources);
}

FullScreenApplicationHandler* FullScreenWindowDetectorMac::GetOrCreateApplicationHandler(
    DesktopCapturer::SourceId source_id) {
  if (app_handler_ == nullptr || app_handler_->GetSourceId() != source_id) {
    app_handler_ = CreateFullScreenMacApplicationHandler(source_id);
  }
  return app_handler_.get();
}

std::unique_ptr<FullScreenWindowDetector>
    FullScreenWindowDetector::CreateFullScreenWindowDetector() {
  return std::make_unique<FullScreenWindowDetectorMac>();
}

}  // namespace webrtc
