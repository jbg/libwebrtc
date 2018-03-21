/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <dlfcn.h>

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include "modules/desktop_capture/mac/screen_capturer_mac_legacy.h"

namespace webrtc {

namespace {

// Definitions used to dynamic-link to deprecated OS 10.6 functions.
const char* kApplicationServicesLibraryName =
    "/System/Library/Frameworks/ApplicationServices.framework/"
    "ApplicationServices";

}  // namespace

ScreenCapturerMacLegacy::ScreenCapturerMacLegacy(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : ScreenCapturerMacBase(desktop_config_monitor) {}

ScreenCapturerMacLegacy::~ScreenCapturerMacLegacy() {
  dlclose(app_services_library_);
}

void ScreenCapturerMacLegacy::ScreenConfigurationChanged() {
  ScreenCapturerMacBase::ScreenConfigurationChanged();

  // Dynamically link to the deprecated pre-Lion capture APIs.
  app_services_library_ = dlopen(kApplicationServicesLibraryName, RTLD_LAZY);
  if (!app_services_library_) {
    LOG_F(LS_ERROR) << "Failed to open " << kApplicationServicesLibraryName;
    abort();
  }

  cg_display_base_address_ = reinterpret_cast<CGDisplayBaseAddressFunc>(
      dlsym(app_services_library_, "CGDisplayBaseAddress"));
  cg_display_bytes_per_row_ = reinterpret_cast<CGDisplayBytesPerRowFunc>(
      dlsym(app_services_library_, "CGDisplayBytesPerRow"));
  cg_display_bits_per_pixel_ = reinterpret_cast<CGDisplayBitsPerPixelFunc>(
      dlsym(app_services_library_, "CGDisplayBitsPerPixel"));
  if (!(cg_display_base_address_ && cg_display_bytes_per_row_ && cg_display_bits_per_pixel_)) {
    LOG_F(LS_ERROR);
    abort();
  }
}

bool ScreenCapturerMacLegacy::Blit(const DesktopFrame& frame,
                                   const DesktopRegion& region,
                                   bool* needs_flip) {
  if (needs_flip) *needs_flip = false;

  CgBlitPreLion(frame, region);

  return true;
}

void ScreenCapturerMacLegacy::CgBlitPreLion(const DesktopFrame& frame,
                                            const DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(), queue_.previous_frame()->data(), frame.stride() * frame.size().height());
  }

  for (size_t i = 0; i < desktop_config_.displays.size(); ++i) {
    const MacDisplayConfiguration& display_config = desktop_config_.displays[i];

    // Use deprecated APIs to determine the display buffer layout.
    assert(cg_display_base_address_ && cg_display_bytes_per_row_ && cg_display_bits_per_pixel_);
    uint8_t* display_base_address =
        reinterpret_cast<uint8_t*>((*cg_display_base_address_)(display_config.id));
    assert(display_base_address);
    int src_bytes_per_row = (*cg_display_bytes_per_row_)(display_config.id);
    int src_bytes_per_pixel = (*cg_display_bits_per_pixel_)(display_config.id) / 8;

    // Determine the display's position relative to the desktop, in pixels.
    DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-desktop_config_.pixel_bounds.left(),
                             -desktop_config_.pixel_bounds.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty()) continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    // Calculate where in the output buffer the display's origin is.
    uint8_t* out_ptr = frame.data() + (display_bounds.left() * src_bytes_per_pixel) +
        (display_bounds.top() * frame.stride());

    // Copy the dirty region from the display buffer into our desktop buffer.
    for (DesktopRegion::Iterator i(copy_region); !i.IsAtEnd(); i.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               i.rect());
    }
  }
}

}  // namespace webrtc
