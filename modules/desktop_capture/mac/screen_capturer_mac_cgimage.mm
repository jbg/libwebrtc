/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/screen_capturer_mac_cgimage.h"

namespace webrtc {

ScreenCapturerMacCGImage::ScreenCapturerMacCGImage(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
    bool detect_updated_region)
    : ScreenCapturerMacBase(desktop_config_monitor, detect_updated_region) {}

ScreenCapturerMacCGImage::~ScreenCapturerMacCGImage() {}

DesktopCapturer::Result ScreenCapturerMacCGImage::BlitDisplayToFrame(CGDirectDisplayID display_id,
                                                                     DesktopRect display_bounds,
                                                                     DesktopRegion copy_region,
                                                                     const DesktopFrame& frame) {
  // Create an image containing a snapshot of the display.
  CGImageRef image = CGDisplayCreateImage(display_id);
  if (!image) return Result::ERROR_TEMPORARY;

  // Verify that the image has 32-bit depth.
  int bits_per_pixel = CGImageGetBitsPerPixel(image);
  if (bits_per_pixel / 8 != DesktopFrame::kBytesPerPixel) {
    RTC_LOG(LS_ERROR) << "CGDisplayCreateImage() returned imaged with " << bits_per_pixel
                      << " bits per pixel. Only 32-bit depth is supported.";
    CFRelease(image);

    return Result::ERROR_PERMANENT;
  }

  // Request access to the raw pixel data via the image's DataProvider.
  CGDataProviderRef provider = CGImageGetDataProvider(image);
  CFDataRef data = CGDataProviderCopyData(provider);
  RTC_CHECK(data);

  const uint8_t* display_base_address = CFDataGetBytePtr(data);
  int src_bytes_per_row = CGImageGetBytesPerRow(image);

  CopyDisplayRegionToFrame(display_bounds,
                           copy_region,
                           display_base_address,
                           src_bytes_per_row,
                           CGImageGetWidth(image),
                           CGImageGetHeight(image),
                           frame);

  CFRelease(data);
  CFRelease(image);

  return Result::SUCCESS;
}

}  // namespace webrtc
