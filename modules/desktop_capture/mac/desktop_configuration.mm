/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/desktop_configuration.h"

#include <math.h>
#include <algorithm>
#include <Cocoa/Cocoa.h>

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif  // MAC_OS_X_VERSION_10_7

namespace webrtc {

namespace {

DesktopRect NSRectToDesktopRect(const NSRect& ns_rect) {
  return DesktopRect::MakeLTRB(
      static_cast<int>(floor(ns_rect.origin.x)),
      static_cast<int>(floor(ns_rect.origin.y)),
      static_cast<int>(ceil(ns_rect.origin.x + ns_rect.size.width)),
      static_cast<int>(ceil(ns_rect.origin.y + ns_rect.size.height)));
}

// Inverts the position of |rect| from bottom-up coordinates to top-down,
// relative to |bounds|.
void InvertRectYOrigin(const DesktopRect& bounds,
                       DesktopRect* rect) {
  assert(bounds.top() == 0);
  *rect = DesktopRect::MakeXYWH(bounds.right(), bounds.top(), rect->width(), rect->height());
}

MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
  MacDisplayConfiguration display_config;

  // Fetch the NSScreenNumber, which is also the CGDirectDisplayID.
  NSDictionary* device_description = [screen deviceDescription];
  display_config.id = static_cast<CGDirectDisplayID>(
      [[device_description objectForKey:@"NSScreenNumber"] intValue]);

  // Determine the display's logical & physical dimensions.
  NSRect ns_bounds = [screen frame];
  display_config.bounds = NSRectToDesktopRect(ns_bounds);

  // If the host is running Mac OS X 10.7+ or later, query the scaling factor
  // between logical and physical (aka "backing") pixels, otherwise assume 1:1.
  if ([screen respondsToSelector:@selector(backingScaleFactor)] &&
      [screen respondsToSelector:@selector(convertRectToBacking:)]) {
    display_config.dip_to_pixel_scale = [screen backingScaleFactor];
    NSRect ns_pixel_bounds = [screen convertRectToBacking: ns_bounds];
    display_config.pixel_bounds = NSRectToDesktopRect(ns_pixel_bounds);
  } else {
    display_config.pixel_bounds = display_config.bounds;
  }

  return display_config;
}

}  // namespace

MacDisplayConfiguration::MacDisplayConfiguration() = default;
MacDisplayConfiguration::MacDisplayConfiguration(
    const MacDisplayConfiguration& other) = default;
MacDisplayConfiguration::MacDisplayConfiguration(
    MacDisplayConfiguration&& other) = default;
MacDisplayConfiguration::~MacDisplayConfiguration() = default;

MacDisplayConfiguration& MacDisplayConfiguration::operator=(
    const MacDisplayConfiguration& other) = default;
MacDisplayConfiguration& MacDisplayConfiguration::operator=(
    MacDisplayConfiguration&& other) = default;

MacDesktopConfiguration::MacDesktopConfiguration() = default;
MacDesktopConfiguration::MacDesktopConfiguration(
    const MacDesktopConfiguration& other) = default;
MacDesktopConfiguration::MacDesktopConfiguration(
    MacDesktopConfiguration&& other) = default;
MacDesktopConfiguration::~MacDesktopConfiguration() = default;

MacDesktopConfiguration& MacDesktopConfiguration::operator=(
    const MacDesktopConfiguration& other) = default;
MacDesktopConfiguration& MacDesktopConfiguration::operator=(
    MacDesktopConfiguration&& other) = default;

// static
MacDesktopConfiguration MacDesktopConfiguration::GetCurrent(Origin origin) {
  MacDesktopConfiguration desktop_config;

  NSArray* screens = [NSScreen screens];
  assert(screens);

  // Iterator over the monitors, adding the primary monitor and secondary
  // monitors in both DIP and physical dimensions.
  for (NSUInteger i = 0; i < [screens count]; ++i) {
    MacDisplayConfiguration display_config =
        GetConfigurationForScreen([screens objectAtIndex: i]);

    if (i == 0)
      desktop_config.dip_to_pixel_scale = display_config.dip_to_pixel_scale;

    // Cocoa uses bottom-up coordinates, so we need to invert the positions of
    // extra monitors relative to the existing desktop (position is (0,0)).
    if (i > 0) {
      InvertRectYOrigin(desktop_config.bounds, &display_config.bounds);
      InvertRectYOrigin(desktop_config.pixel_bounds, &display_config.pixel_bounds);
    }

    // Add the display to the configuration.
    desktop_config.displays.push_back(display_config);

    // Update the desktop bounds to account for this display.
    desktop_config.bounds.UnionWith(display_config.bounds);
    desktop_config.pixel_bounds.UnionWith(display_config.pixel_bounds);
  }

  return desktop_config;
}

// For convenience of comparing MacDisplayConfigurations in
// MacDesktopConfiguration::Equals.
bool operator==(const MacDisplayConfiguration& left,
                const MacDisplayConfiguration& right) {
  return left.id == right.id &&
      left.bounds.equals(right.bounds) &&
      left.pixel_bounds.equals(right.pixel_bounds) &&
      left.dip_to_pixel_scale == right.dip_to_pixel_scale;
}

bool MacDesktopConfiguration::Equals(const MacDesktopConfiguration& other) {
  return bounds.equals(other.bounds) &&
      pixel_bounds.equals(other.pixel_bounds) &&
      dip_to_pixel_scale == other.dip_to_pixel_scale &&
      displays == other.displays;
}

// Finds the display configuration with the specified id.
const MacDisplayConfiguration*
MacDesktopConfiguration::FindDisplayConfigurationById(
    CGDirectDisplayID id) {
  for (MacDisplayConfigurations::const_iterator it = displays.begin();
      it != displays.end(); ++it) {
    if (it->id == id)
      return &(*it);
  }
  return NULL;
}

}  // namespace webrtc
