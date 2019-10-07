/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_
#define MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_

#include <memory>
#include "api/function_view.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/full_screen_application_handler.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

// This is a way to handle switch to full-screen mode for application in some
// specific cases:
// Chrome on MacOS creates a new window in full-screen mode to
// show a tab full-screen and minimizes the old window.
// PowerPoint creates new windows in full-screen mode when user goes to
// presentation mode (Slide Show Window, Presentation Window). To continue
// capturing in these cases, we try to find the new full-screen window using
// criteria provided by application specific FullScreenApplicationHandler.

class FullScreenWindowDetector : public rtc::RefCountedBase {
 public:
  FullScreenWindowDetector();

  // Returns the full-screen window in place of the original window if all the
  // criteria provided by FullScreenApplicationHandler are met, or 0 if no such
  // window found.
  virtual DesktopCapturer::SourceId FindFullScreenWindow(
      DesktopCapturer::SourceId original_source_id);

  // The caller should call this function periodically, implementation will
  // update internal state no often than twice per second
  void UpdateWindowListIfNeeded(
      DesktopCapturer::SourceId original_source_id,
      rtc::FunctionView<bool(DesktopCapturer::SourceList*)> get_sources);

  static rtc::scoped_refptr<FullScreenWindowDetector>
  CreateFullScreenWindowDetector();

 protected:
  // This method should be overriden by platform specific implementation.
  // General approach is
  // - to find owner process id for window presented by source_id
  // - to create a FullScreenApplicationHandler specific for application (Google
  // Chrome, PowerPoint, etc.)
  virtual void CreateApplicationHandlerIfNeeded(
      DesktopCapturer::SourceId source_id);

 protected:
  std::unique_ptr<FullScreenApplicationHandler> app_handler_;

 private:
  int64_t last_update_time_ms_;
  DesktopCapturer::SourceId previous_source_id_;
  // We cache the last two results of the window list, so
  // |previous_window_list_| is taken at least 500ms before the next Capture()
  // call. If we only save the last result, we may get false positive (i.e.
  // full-screen window exists in the list) if Capture() is called too soon.
  DesktopCapturer::SourceList current_window_list_;
  DesktopCapturer::SourceList previous_window_list_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FullScreenWindowDetector);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_
