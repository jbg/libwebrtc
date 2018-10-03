/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capture_buildflags.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"

#if BUILDFLAG(USE_PIPEWIRE)
#include "modules/desktop_capture/screen_capturer_pipewire.cc"
#endif  // BUILDFLAG(USE_PIPEWIRE)

#if defined(USE_X11)
#include "modules/desktop_capture/screen_capturer_x11.cc"
#endif  // defined(USE_X11)

namespace webrtc {

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
#if BUILDFLAG(USE_PIPEWIRE)
  if (DesktopCapturer::IsRunningUnderWayland()) {
    return ScreenCapturerPipeWire::CreateRawScreenCapturer(options);
  }
#endif  // BUILDFLAG(USE_PIPEWIRE)

#if defined(USE_X11)
  return ScreenCapturerX11::CreateRawScreenCapturer(options);
#endif  // defined(USE_X11)

  return nullptr;
}

}  // namespace webrtc
