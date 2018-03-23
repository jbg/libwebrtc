/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/mac/screen_capturer_mac_cgimage.h"
#include "modules/desktop_capture/mac/screen_capturer_mac_iosurface.h"

namespace webrtc {

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.configuration_monitor())
    return nullptr;

  std::unique_ptr<ScreenCapturerMacBase> capturer;

  if (options.allow_iosurface_capturer()) {
    capturer.reset(new ScreenCapturerMacIOSurface(options.configuration_monitor(),
                                                  options.detect_updated_region()));
  } else {
    capturer.reset(new ScreenCapturerMacCGImage(options.configuration_monitor(),
                                                options.detect_updated_region()));
  }

  if (!capturer.get()->Init()) {
    return nullptr;
  }

  return capturer;
}

}  // namespace webrtc