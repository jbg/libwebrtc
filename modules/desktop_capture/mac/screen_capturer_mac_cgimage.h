/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_CGIMAGE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_CGIMAGE_H_

#include "modules/desktop_capture/mac/screen_capturer_mac_base.h"

namespace webrtc {

// ScreenCapturerMacCGImage captures 32bit RGBA using CGDisplayCreateImage.
class ScreenCapturerMacCGImage : public ScreenCapturerMacBase {
 public:
  explicit ScreenCapturerMacCGImage(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
      bool detect_updated_region);
  ~ScreenCapturerMacCGImage() override;

 private:
  DesktopCapturer::Result BlitDisplayToFrame(
      CGDirectDisplayID display_id,
      DesktopRect display_bounds,
      DesktopRegion copy_region,
      const DesktopFrame& frame) override;
  void DisplayRefresh(CGDirectDisplayID display_id,
                      uint64_t display_time,
                      IOSurfaceRef io_surface) override {}

  void ReleaseResources() override {}

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacCGImage);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_CGIMAGE_H_
