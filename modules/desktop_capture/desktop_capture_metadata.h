/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_METADATA_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_METADATA_H_

#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"

namespace webrtc {

// Container for the metadata associated with a desktop capturer.
struct DesktopCaptureMetadata {
  // Details about the XDG desktop session handle (used by wayland
  // implementation in remoting)
  xdg_portal::SessionDetails session_details;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_METADATA_H_
