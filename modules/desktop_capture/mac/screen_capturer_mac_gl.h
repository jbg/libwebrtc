/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_GL_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_GL_H_

#include "modules/desktop_capture/mac/screen_capturer_mac_legacy.h"

namespace webrtc {

typedef CGLError (*CGLSetFullScreenFunc)(CGLContextObj);

// ScreenCapturerMacCGImage captures 32bit RGBA using OpenGL.
class ScreenCapturerMacGL : public ScreenCapturerMacLegacy {
 public:
  // Whether the system supports OpenGL based capturing.
  static bool IsSupported();

  explicit ScreenCapturerMacGL(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  ~ScreenCapturerMacGL() override;

 private:
  void ScreenConfigurationChanged() override;
  bool Blit(const DesktopFrame& frame,
            const DesktopRegion& region,
            bool* needs_flip) override;

  void GlBlitFast(const DesktopFrame& frame, const DesktopRegion& region);
  void GlBlitSlow(const DesktopFrame& frame);

  void DestroyContext();

  void* opengl_library_ = nullptr;
  CGLSetFullScreenFunc cgl_set_full_screen_ = nullptr;
  CGLContextObj cgl_context_ = nullptr;
  ScopedPixelBufferObject pixel_buffer_object_;

  // Contains an invalid region from the previous capture.
  DesktopRegion last_invalid_region_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacGL);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_GL_H_
