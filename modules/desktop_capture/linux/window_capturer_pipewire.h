/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WINDOW_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WINDOW_CAPTURER_PIPEWIRE_H_

#include <memory>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class WindowCapturerPipeWire : public DesktopCapturer {
 public:
  WindowCapturerPipeWire();
  ~WindowCapturerPipeWire() override;

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  Callback* callback_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerPipeWire);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WINDOW_CAPTURER_PIPEWIRE_H_
