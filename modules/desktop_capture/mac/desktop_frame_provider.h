/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_FRAME_PROVIDER_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_FRAME_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>

#include <map>

#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/synchronization/rw_lock_wrapper.h"
#include "sdk/objc/Framework/Classes/Common/scoped_cftyperef.h"

namespace webrtc {

class DesktopFrameProvider {
 public:
  explicit DesktopFrameProvider(bool allow_iosurface);
  ~DesktopFrameProvider();

  std::unique_ptr<SharedDesktopFrame> LatestFrameForDisplay(
      CGDirectDisplayID display_id);
  void InvalidateIOSurface(CGDirectDisplayID display_id,
                           rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface);
  void InvalidateAllIOSurfaces();

 private:
  const bool allow_iosurface_;

  // A lock protecting |io_surfaces_| across threads.
  std::unique_ptr<RWLockWrapper> io_surfaces_lock_;

  // Most recent IOSurface that contains a capture of matching display.
  std::map<CGDirectDisplayID, std::unique_ptr<SharedDesktopFrame>> io_surfaces_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopFrameProvider);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_FRAME_PROVIDER_H_
