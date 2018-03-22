/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_IOSURFACE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_IOSURFACE_H_

#include <IOSurface/IOSurface.h>

#include <map>
#include <memory>

#include "modules/desktop_capture/mac/screen_capturer_mac_base.h"

namespace webrtc {

// ScreenCapturerMacIOSurface captures 32bit RGBA using IOSurface.
class ScreenCapturerMacIOSurface : public ScreenCapturerMacBase {
 public:
  explicit ScreenCapturerMacIOSurface(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
      bool detect_updated_region);
  ~ScreenCapturerMacIOSurface() override;

 private:
  DesktopCapturer::Result BlitDisplayToFrame(
      CGDirectDisplayID display_id,
      DesktopRect display_bounds,
      DesktopRegion copy_region,
      const DesktopFrame& frame) override;

  void DisplayRefresh(CGDirectDisplayID display_id,
                      uint64_t display_time,
                      IOSurfaceRef io_surface) override;

  void ReleaseResources() override;

  // A lock protecting |io_surfaces_| across threads.
  std::unique_ptr<RWLockWrapper> io_surfaces_lock_;

  // Most recent IOSurface that contains a capture of matching display.
  std::map<CGDirectDisplayID, rtc::ScopedCFTypeRef<IOSurfaceRef>> io_surfaces_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacIOSurface);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_IOSURFACE_H_
