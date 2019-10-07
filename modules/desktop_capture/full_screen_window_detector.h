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

class FullScreenWindowDetector : public rtc::RefCountedBase {
 public:
  FullScreenWindowDetector();

  virtual DesktopCapturer::SourceId FindFullScreenWindow(
      DesktopCapturer::SourceId original_source_id);

  void UpdateWindowListIfNeeded(
      DesktopCapturer::SourceId original_source_id,
      rtc::FunctionView<bool(DesktopCapturer::SourceList*)> get_sources);

  static rtc::scoped_refptr<FullScreenWindowDetector>
  CreateFullScreenWindowDetector();

 protected:
  virtual bool UpdateWindowList(
      DesktopCapturer::SourceId original_source_id,
      DesktopCapturer::SourceList* sources,
      rtc::FunctionView<bool(DesktopCapturer::SourceList*)> get_sources);

  virtual FullScreenApplicationHandler* GetOrCreateApplicationHandler(
      DesktopCapturer::SourceId source_id);

 protected:
  std::unique_ptr<FullScreenApplicationHandler> app_handler_;

 private:
  int64_t last_update_time_ns_;
  DesktopCapturer::SourceId previous_source_id_;
  DesktopCapturer::SourceList current_window_list_;
  DesktopCapturer::SourceList previous_window_list_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FullScreenWindowDetector);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_
