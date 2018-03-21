/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include "modules/desktop_capture/mac/screen_capturer_mac_cgimage.h"

namespace webrtc {

namespace {

// Returns an array of CGWindowID for all the on-screen windows except
// |window_to_exclude|, or NULL if the window is not found or it fails. The
// caller should release the returned CFArrayRef.
CFArrayRef CreateWindowListWithExclusion(CGWindowID window_to_exclude) {
  if (!window_to_exclude) return nullptr;

  CFArrayRef all_windows =
      CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  if (!all_windows) return nullptr;

  CFMutableArrayRef returned_array =
      CFArrayCreateMutable(nullptr, CFArrayGetCount(all_windows), nullptr);

  bool found = false;
  for (CFIndex i = 0; i < CFArrayGetCount(all_windows); ++i) {
    CFDictionaryRef window =
        reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(all_windows, i));

    CFNumberRef id_ref =
        reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(window, kCGWindowNumber));

    CGWindowID id;
    CFNumberGetValue(id_ref, kCFNumberIntType, &id);
    if (id == window_to_exclude) {
      found = true;
      continue;
    }
    CFArrayAppendValue(returned_array, reinterpret_cast<void*>(id));
  }
  CFRelease(all_windows);

  if (!found) {
    CFRelease(returned_array);
    returned_array = nullptr;
  }
  return returned_array;
}

// Create an image of the given region using the given |window_list|.
// |pixel_bounds| should be in the primary display's coordinate in physical
// pixels. The caller should release the returned CGImageRef and CFDataRef.
CGImageRef CreateExcludedWindowRegionImage(const DesktopRect& pixel_bounds,
                                           float dip_to_pixel_scale,
                                           CFArrayRef window_list) {
  CGRect window_bounds;
  // The origin is in DIP while the size is in physical pixels. That's what
  // CGWindowListCreateImageFromArray expects.
  window_bounds.origin.x = pixel_bounds.left() / dip_to_pixel_scale;
  window_bounds.origin.y = pixel_bounds.top() / dip_to_pixel_scale;
  window_bounds.size.width = pixel_bounds.width();
  window_bounds.size.height = pixel_bounds.height();

  return CGWindowListCreateImageFromArray(window_bounds, window_list, kCGWindowImageDefault);
}

// Scales all coordinates of a rect by a specified factor.
DesktopRect ScaleAndRoundCGRect(const CGRect& rect, float scale) {
  return DesktopRect::MakeLTRB(static_cast<int>(floor(rect.origin.x * scale)),
                               static_cast<int>(floor(rect.origin.y * scale)),
                               static_cast<int>(ceil((rect.origin.x + rect.size.width) * scale)),
                               static_cast<int>(ceil((rect.origin.y + rect.size.height) * scale)));
}

// Returns the bounds of |window| in physical pixels, enlarged by a small amount
// on four edges to take account of the border/shadow effects.
DesktopRect GetExcludedWindowPixelBounds(CGWindowID window, float dip_to_pixel_scale) {
  // The amount of pixels to add to the actual window bounds to take into
  // account of the border/shadow effects.
  static const int kBorderEffectSize = 20;
  CGRect rect;
  CGWindowID ids[1];
  ids[0] = window;

  CFArrayRef window_id_array =
      CFArrayCreate(nullptr, reinterpret_cast<const void**>(&ids), 1, nullptr);
  CFArrayRef window_array = CGWindowListCreateDescriptionFromArray(window_id_array);

  if (CFArrayGetCount(window_array) > 0) {
    CFDictionaryRef window =
        reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(window_array, 0));
    CFDictionaryRef bounds_ref =
        reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(window, kCGWindowBounds));
    CGRectMakeWithDictionaryRepresentation(bounds_ref, &rect);
  }

  CFRelease(window_id_array);
  CFRelease(window_array);

  rect.origin.x -= kBorderEffectSize;
  rect.origin.y -= kBorderEffectSize;
  rect.size.width += kBorderEffectSize * 2;
  rect.size.height += kBorderEffectSize * 2;
  // |rect| is in DIP, so convert to physical pixels.
  return ScaleAndRoundCGRect(rect, dip_to_pixel_scale);
}

}  // namespace

// static
bool ScreenCapturerMacCGImage::IsSupported() {
  // CgBlitPostLion uses CGDisplayCreateImage() to snapshot each display's
  // contents. Although the API exists in OS 10.6, it crashes the caller if
  // the machine has no monitor connected, so we fall back to depcreated APIs
  // when running on 10.6.
  return rtc::GetOSVersionName() >= rtc::kMacOSLion;
}

ScreenCapturerMacCGImage::ScreenCapturerMacCGImage(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : ScreenCapturerMacBase(desktop_config_monitor) {}

ScreenCapturerMacCGImage::~ScreenCapturerMacCGImage() {}

void ScreenCapturerMacCGImage::SetExcludedWindow(WindowId window) {
  excluded_window_ = window;
}

void ScreenCapturerMacCGImage::ScreenConfigurationChanged() {
  ScreenCapturerMacBase::ScreenConfigurationChanged();

  if (!IsSupported()) abort();

  RTC_LOG(LS_INFO) << "Using CgBlitPostLion.";
}

bool ScreenCapturerMacCGImage::Blit(const DesktopFrame& frame,
                                    const DesktopRegion& region,
                                    bool* needs_flip) {
  if (needs_flip) *needs_flip = false;

  // Lion requires us to use their new APIs for doing screen capture. These
  // APIS currently crash on 10.6.8 if there is no monitor attached.
  return CgBlitPostLion(frame, region);
}

void ScreenCapturerMacCGImage::CopyDisplayRegionToFrame(DesktopRect display_bounds,
                                                        DesktopRegion copy_region,
                                                        const uint8_t* display_base_address,
                                                        int src_bytes_per_row,
                                                        int src_width,
                                                        int src_height,
                                                        const DesktopFrame& frame) {
  // |image| size may be different from display_bounds in case the screen was
  // resized recently.
  copy_region.IntersectWith(DesktopRect::MakeWH(src_width, src_height));

  // Copy the dirty region from the display buffer into our desktop buffer.
  uint8_t* out_ptr = frame.GetFrameDataAtPos(display_bounds.top_left());
  for (DesktopRegion::Iterator i(copy_region); !i.IsAtEnd(); i.Advance()) {
    CopyRect(display_base_address,
             src_bytes_per_row,
             out_ptr,
             frame.stride(),
             DesktopFrame::kBytesPerPixel,
             i.rect());
  }
}

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
  assert(data);

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

bool ScreenCapturerMacCGImage::CgBlitPostLion(const DesktopFrame& frame,
                                              const DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(), queue_.previous_frame()->data(), frame.stride() * frame.size().height());
  }

  MacDisplayConfigurations displays_to_capture;
  if (current_display_) {
    // Capturing a single screen. Note that the screen id may change when
    // screens are added or removed.
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    if (config) {
      displays_to_capture.push_back(*config);
    } else {
      RTC_LOG(LS_ERROR) << "The selected screen cannot be found for capturing.";
      return false;
    }
  } else {
    // Capturing the whole desktop.
    displays_to_capture = desktop_config_.displays;
  }

  // Create the window list once for all displays.
  CFArrayRef window_list = CreateWindowListWithExclusion(excluded_window_);

  for (size_t i = 0; i < displays_to_capture.size(); ++i) {
    const MacDisplayConfiguration& display_config = displays_to_capture[i];

    // Capturing mixed-DPI on one surface is hard, so we only return displays
    // that match the "primary" display's DPI. The primary display is always
    // the first in the list.
    if (i > 0 && display_config.dip_to_pixel_scale != displays_to_capture[0].dip_to_pixel_scale) {
      continue;
    }
    // Determine the display's position relative to the desktop, in pixels.
    DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-screen_pixel_bounds_.left(), -screen_pixel_bounds_.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty()) continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    DesktopRect excluded_window_bounds;
    CGImageRef excluded_image = nullptr;
    if (excluded_window_ && window_list) {
      // Get the region of the excluded window relative the primary display.
      excluded_window_bounds =
          GetExcludedWindowPixelBounds(excluded_window_, display_config.dip_to_pixel_scale);
      excluded_window_bounds.IntersectWith(display_config.pixel_bounds);

      // Create the image under the excluded window first, because it's faster
      // than captuing the whole display.
      if (!excluded_window_bounds.is_empty()) {
        excluded_image = CreateExcludedWindowRegionImage(
            excluded_window_bounds, display_config.dip_to_pixel_scale, window_list);
      }
    }

    DesktopCapturer::Result result =
        BlitDisplayToFrame(display_config.id, display_bounds, copy_region, frame);
    if (result != Result::SUCCESS) {
      if (excluded_image) CFRelease(excluded_image);
    }

    if (result == Result::ERROR_TEMPORARY) continue;

    if (result == Result::ERROR_PERMANENT) return false;

    if (excluded_image) {
      CGDataProviderRef provider = CGImageGetDataProvider(excluded_image);
      CFDataRef excluded_image_data = CGDataProviderCopyData(provider);
      assert(excluded_image_data);
      const uint8_t* display_base_address = CFDataGetBytePtr(excluded_image_data);
      int src_bytes_per_row = CGImageGetBytesPerRow(excluded_image);

      // Translate the bounds relative to the desktop, because |frame| data
      // starts from the desktop top-left corner.
      DesktopRect window_bounds_relative_to_desktop(excluded_window_bounds);
      window_bounds_relative_to_desktop.Translate(-screen_pixel_bounds_.left(),
                                                  -screen_pixel_bounds_.top());

      DesktopRect rect_to_copy = DesktopRect::MakeSize(excluded_window_bounds.size());
      rect_to_copy.IntersectWith(
          DesktopRect::MakeWH(CGImageGetWidth(excluded_image), CGImageGetHeight(excluded_image)));

      if (CGImageGetBitsPerPixel(excluded_image) / 8 == DesktopFrame::kBytesPerPixel) {
        CopyRect(display_base_address,
                 src_bytes_per_row,
                 frame.GetFrameDataAtPos(window_bounds_relative_to_desktop.top_left()),
                 frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 rect_to_copy);
      }

      CFRelease(excluded_image_data);
      CFRelease(excluded_image);
    }
  }
  if (window_list) CFRelease(window_list);
  return true;
}

}  // namespace webrtc
