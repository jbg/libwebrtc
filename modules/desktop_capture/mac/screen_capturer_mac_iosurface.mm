/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/screen_capturer_mac_iosurface.h"

namespace webrtc {

// static
bool ScreenCapturerMacIOSurface::IsSupported() {
  return rtc::GetOSVersionName() >= rtc::kMacOSLion;
}

ScreenCapturerMacIOSurface::ScreenCapturerMacIOSurface(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : ScreenCapturerMacCGImage(desktop_config_monitor),
      io_surfaces_lock_(RWLockWrapper::CreateRWLock()) {}

ScreenCapturerMacIOSurface::~ScreenCapturerMacIOSurface() {
  ReleaseIOSurfaces();
}

void ScreenCapturerMacIOSurface::ScreenConfigurationChanged() {
  ScreenCapturerMacBase::ScreenConfigurationChanged();

  ReleaseIOSurfaces();

  if (!IsSupported()) abort();

  RTC_LOG(LS_INFO) << "Using IOSurface.";
}

void ScreenCapturerMacIOSurface::ScreenRefresh(CGRectCount count,
                                               const CGRect* rect_array,
                                               DesktopVector display_origin,
                                               CGDirectDisplayID display_id,
                                               IOSurfaceRef io_surface) {
  ScreenCapturerMacCGImage::ScreenRefresh(count, rect_array, display_origin, display_id, nullptr);

  WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);

  // Decrement use count before the surface is released.
  if (io_surfaces_[display_id]) {
    IOSurfaceDecrementUseCount(io_surfaces_[display_id].get());
    io_surfaces_[display_id].reset(nullptr);
  }

  // Insert the new surface.
  if (io_surface) {
    io_surfaces_[display_id].reset(io_surface, rtc::RetainPolicy::RETAIN);
    IOSurfaceIncrementUseCount(io_surfaces_[display_id].get());
  }
}

DesktopCapturer::Result ScreenCapturerMacIOSurface::BlitDisplayToFrame(CGDirectDisplayID display_id,
                                                                       DesktopRect display_bounds,
                                                                       DesktopRegion copy_region,
                                                                       const DesktopFrame& frame) {
  rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  {
    WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);
    if (io_surfaces_[display_id]) {
      io_surface = io_surfaces_[display_id];
      IOSurfaceIncrementUseCount(io_surface.get());
    }
  }

  if (!io_surface) {
    RTC_LOG(LS_INFO) << "Fallback to cgimage because no io surface for display " << display_id;
    return ScreenCapturerMacCGImage::BlitDisplayToFrame(
        display_id, display_bounds, copy_region, frame);
  }

  // Grant cpu access to the underlying data.
  IOReturn status = IOSurfaceLock(io_surface.get(), 0, nullptr);
  if (status != kIOReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed lock the IOSurface " << status;
    IOSurfaceDecrementUseCount(io_surface.get());
    return Result::ERROR_TEMPORARY;
  }

  const uint8_t* display_base_address =
      reinterpret_cast<uint8_t*>(IOSurfaceGetBaseAddress(io_surface.get()));
  int src_bytes_per_row = IOSurfaceGetBytesPerRow(io_surface.get());
  int bytes_per_pixel = IOSurfaceGetBytesPerElement(io_surface.get());

  // Verify that the IOSurface has 32-bit depth.
  if (bytes_per_pixel != DesktopFrame::kBytesPerPixel) {
    RTC_LOG(LS_ERROR) << "The current IOSurface has " << 8 * bytes_per_pixel
                      << " bits per pixel. Only 32-bit depth is supported.";
    IOSurfaceUnlock(io_surface.get(), 0, nullptr);
    IOSurfaceDecrementUseCount(io_surface.get());

    return Result::ERROR_PERMANENT;
  }

  int src_width = IOSurfaceGetWidth(io_surface.get());
  int src_height = IOSurfaceGetHeight(io_surface.get());

  CopyDisplayRegionToFrame(display_bounds,
                           copy_region,
                           display_base_address,
                           src_bytes_per_row,
                           src_width,
                           src_height,
                           frame);

  IOSurfaceUnlock(io_surface.get(), 0, nullptr);
  IOSurfaceDecrementUseCount(io_surface.get());

  return Result::SUCCESS;
}

void ScreenCapturerMacIOSurface::ReleaseIOSurfaces() {
  WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);
  for (auto itr = io_surfaces_.begin(); itr != io_surfaces_.end(); ++itr) {
    if (itr->second) IOSurfaceDecrementUseCount(itr->second.get());
  }
  io_surfaces_.clear();
}

}  // namespace webrtc
