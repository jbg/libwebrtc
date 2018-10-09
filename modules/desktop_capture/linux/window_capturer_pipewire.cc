/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/window_capturer_pipewire.h"

#include <assert.h>

#include <memory>

namespace webrtc {

WindowCapturerPipeWire::WindowCapturerPipeWire() {}
WindowCapturerPipeWire::~WindowCapturerPipeWire() {}

bool WindowCapturerPipeWire::GetSourceList(SourceList* sources) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerPipeWire::SelectSource(SourceId id) {
  // Not implemented yet.
  return false;
}

void WindowCapturerPipeWire::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerPipeWire::CaptureFrame() {
  // Not implemented yet.
  callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
}

// static
std::unique_ptr<DesktopCapturer>
WindowCapturerPipeWire::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerPipeWire());
}

}  // namespace webrtc
