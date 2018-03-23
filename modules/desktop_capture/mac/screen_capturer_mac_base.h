/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_BASE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_BASE_H_

#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/macutils.h"
#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/Common/scoped_cftyperef.h"

namespace webrtc {

class DisplayStreamManager;

// A class to perform video frame capturing for mac.
class ScreenCapturerMacBase : public DesktopCapturer {
 public:
  explicit ScreenCapturerMacBase(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
      bool detect_updated_region);
  ~ScreenCapturerMacBase() override;

  bool Init();

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  void SetExcludedWindow(WindowId window) override;
  bool GetSourceList(SourceList* screens) override;
  bool SelectSource(SourceId id) override;

 protected:
  // Blit specific display into a frame.
  // Return ERROR_TEMPORARY to skip the display.
  // Return ERROR_PERMANENT to fail the capture.
  // Otherwise return SUCCESS.
  virtual DesktopCapturer::Result BlitDisplayToFrame(
      CGDirectDisplayID display_id,
      DesktopRect display_bounds,
      DesktopRegion copy_region,
      const DesktopFrame& frame) = 0;

  virtual void DisplayRefresh(CGDirectDisplayID display_id,
                              uint64_t display_time,
                              IOSurfaceRef io_surface) = 0;

  virtual void ReleaseResources() = 0;

  // Helper to copy regions of display into a frame using raw pointer.
  void CopyDisplayRegionToFrame(DesktopRect display_bounds,
                                DesktopRegion copy_region,
                                const uint8_t* display_base_address,
                                int src_bytes_per_row,
                                int src_width,
                                int src_height,
                                const DesktopFrame& frame);

 private:
  // Returns false if the selected screen is no longer valid.
  bool Blit(const DesktopFrame& frame, const DesktopRegion& region);

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ScreenRefresh(CGRectCount count,
                     const CGRect* rect_array,
                     DesktopVector display_origin);
  void ReleaseBuffers();

  std::unique_ptr<DesktopFrame> CreateFrame();

  const bool detect_updated_region_;

  Callback* callback_ = nullptr;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  // Current display configuration.
  MacDesktopConfiguration desktop_config_;

  // Currently selected display, or 0 if the full desktop is selected. On OS X
  // 10.6 and before, this is always 0.
  CGDirectDisplayID current_display_ = 0;

  // The physical pixel bounds of the current screen.
  DesktopRect screen_pixel_bounds_;

  // The dip to physical pixel scale of the current screen.
  float dip_to_pixel_scale_ = 1.0f;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Contains an invalid region from the previous capture.
  DesktopRegion last_invalid_region_;

  // Monitoring display reconfiguration.
  rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor_;

  CGWindowID excluded_window_ = 0;

  // A self-owned object that will destroy itself after ScreenCapturerMac and
  // all display streams have been destroyed..
  DisplayStreamManager* display_stream_manager_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacBase);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_BASE_H_
