/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_LEGACY_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_LEGACY_H_

#include "modules/desktop_capture/mac/screen_capturer_mac_base.h"

namespace webrtc {

typedef void* (*CGDisplayBaseAddressFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBytesPerRowFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBitsPerPixelFunc)(CGDirectDisplayID);

// ScreenCapturerMacLegacy captures 32bit RGBA using legacy api.
class ScreenCapturerMacLegacy : public ScreenCapturerMacBase {
 public:
  explicit ScreenCapturerMacLegacy(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  ~ScreenCapturerMacLegacy() override;

 protected:
  void ScreenConfigurationChanged() override;
  bool Blit(const DesktopFrame& frame,
            const DesktopRegion& region,
            bool* needs_flip) override;

 private:
  void CgBlitPreLion(const DesktopFrame& frame, const DesktopRegion& region);

  // Dynamically link to deprecated APIs for Mac OS X 10.6 support.
  void* app_services_library_ = nullptr;
  CGDisplayBaseAddressFunc cg_display_base_address_ = nullptr;
  CGDisplayBytesPerRowFunc cg_display_bytes_per_row_ = nullptr;
  CGDisplayBitsPerPixelFunc cg_display_bits_per_pixel_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacLegacy);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_LEGACY_H_
