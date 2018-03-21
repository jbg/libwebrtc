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
  // Whether the system supports CGImage based capturing.
  static bool IsSupported();

  explicit ScreenCapturerMacCGImage(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  ~ScreenCapturerMacCGImage() override;

  // DesktopCapturer interface.
  void SetExcludedWindow(WindowId window) override;

 protected:
  void CopyDisplayRegionToFrame(DesktopRect display_bounds,
                                DesktopRegion copy_region,
                                const uint8_t* display_base_address,
                                int src_bytes_per_row,
                                int src_width,
                                int src_height,
                                const DesktopFrame& frame);
  virtual DesktopCapturer::Result BlitDisplayToFrame(
      CGDirectDisplayID display_id,
      DesktopRect display_bounds,
      DesktopRegion copy_region,
      const DesktopFrame& frame);

  void ScreenConfigurationChanged() override;
  bool Blit(const DesktopFrame& frame,
            const DesktopRegion& region,
            bool* needs_flip) override;

 private:
  // Returns false if the selected screen is no longer valid.
  bool CgBlitPostLion(const DesktopFrame& frame, const DesktopRegion& region);

  CGWindowID excluded_window_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacCGImage);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_CGIMAGE_H_
