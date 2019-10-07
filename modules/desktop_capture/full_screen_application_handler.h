/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_
#define MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_

#include <memory>
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class FullScreenApplicationHandler {
 public:
  virtual ~FullScreenApplicationHandler() {}

  FullScreenApplicationHandler();

  virtual bool CanHandleFullScreen() const;

  virtual DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& previousWindowList,
      const DesktopCapturer::SourceList& currentWindowList) const;

  DesktopCapturer::SourceId GetSourceId() const;

  void SetSourceId(DesktopCapturer::SourceId sourceId);

 private:
  DesktopCapturer::SourceId sourceId_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FullScreenApplicationHandler);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_
