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

#include <memory>

#include "base/checks.h"
#include "base/constructormagic.h"
#include "base/logging.h"
#include "base/macutils.h"
#include "base/timeutils.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "modules/desktop_capture/mac/scoped_pixel_buffer_object.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"

namespace webrtc {

class DisplayStreamManager;

// A base class to perform video frame capturing for mac.
class ScreenCapturerMacBase : public DesktopCapturer {
 public:
  explicit ScreenCapturerMacBase(
      rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  ~ScreenCapturerMacBase() override;

  bool Init();

  // Called when the screen configuration is changed.
  virtual void ScreenConfigurationChanged();

  virtual bool Blit(const DesktopFrame& frame,
                    const DesktopRegion& region,
                    bool* needs_flip) = 0;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* screens) override;
  bool SelectSource(SourceId id) override;

 protected:
  virtual void ScreenRefresh(CGRectCount count,
                             const CGRect* rect_array,
                             DesktopVector display_origin,
                             CGDirectDisplayID display_id,
                             IOSurfaceRef io_surface);

  void CopyRect(const uint8_t* src_plane,
                int src_plane_stride,
                uint8_t* dest_plane,
                int dest_plane_stride,
                int bytes_per_pixel,
                const DesktopRect& rect) const;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  // Current display configuration.
  MacDesktopConfiguration desktop_config_;

  // Currently selected display, or 0 if the full desktop is selected. On OS X
  // 10.6 and before, this is always 0.
  CGDirectDisplayID current_display_ = 0;

  // The physical pixel bounds of the current screen.
  DesktopRect screen_pixel_bounds_;

 private:
  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ReleaseBuffers();

  std::unique_ptr<DesktopFrame> CreateFrame();

  Callback* callback_ = nullptr;

  // The dip to physical pixel scale of the current screen.
  float dip_to_pixel_scale_ = 1.0f;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Monitoring display reconfiguration.
  rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor_;

  // A self-owned object that will destroy itself after ScreenCapturerMac and
  // all display streams have been destroyed..
  DisplayStreamManager* display_stream_manager_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMacBase);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_BASE_H_
