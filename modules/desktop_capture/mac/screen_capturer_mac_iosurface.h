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

#include <map>
#include <memory>

#include <IOSurface/IOSurface.h>

#include "modules/desktop_capture/mac/screen_capturer_mac_cgimage.h"
#include "sdk/objc/Framework/Classes/Common/scoped_cftyperef.h"

namespace webrtc {

// ScreenCapturerMacIOSurface captures 32bit RGBA using IOSurface.
class ScreenCapturerMacIOSurface : public ScreenCapturerMacCGImage {
 public:
  // Whether the system supports IOSurface based capturing.
  static bool IsSupported();

  explicit ScreenCapturerMacIOSurface(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  ~ScreenCapturerMacIOSurface() override;

 private:
  void ScreenConfigurationChanged() override;
  DesktopCapturer::Result BlitDisplayToFrame(
      CGDirectDisplayID display_id,
      DesktopRect display_bounds,
      DesktopRegion copy_region,
      const DesktopFrame& frame) override;

  /*bool Blit(const DesktopFrame& frame,
                    const DesktopRegion& region,
                    bool* needs_flip) override;*/
  void ScreenRefresh(CGRectCount count,
                     const CGRect* rect_array,
                     DesktopVector display_origin,
                     CGDirectDisplayID display_id,
                     IOSurfaceRef io_surface) override;

 private:
  void ReleaseIOSurfaces();

  // A lock protecting |io_surfaces_| across threads.
  std::unique_ptr<RWLockWrapper> io_surfaces_lock_;

  // Most recent IOSurface that contains a capture of matching display.
  std::map<CGDirectDisplayID, rtc::ScopedCFTypeRef<IOSurfaceRef>> io_surfaces_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacIOSurface);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_IOSURFACE_H_
