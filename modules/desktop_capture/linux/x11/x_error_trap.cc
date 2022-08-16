/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/x11/x_error_trap.h"

#include <stddef.h>

namespace webrtc {

namespace {

static int g_last_xserver_error_code = 0;

Mutex* AcquireMutex() {
  static Mutex* mutex = new Mutex();
  return mutex;
}

int XServerErrorHandler(Display* display, XErrorEvent* error_event) {
  g_last_xserver_error_code = error_event->error_code;
  return 0;
}

}  // namespace

XErrorTrap::XErrorTrap() : mutex_lock_(AcquireMutex()) {
  original_error_handler_ = XSetErrorHandler(&XServerErrorHandler);
  g_last_xserver_error_code = 0;
}

int XErrorTrap::GetLastErrorAndDisable() {
  enabled_ = false;
  XSetErrorHandler(original_error_handler_);
  return g_last_xserver_error_code;
}

XErrorTrap::~XErrorTrap() {
  if (enabled_)
    GetLastErrorAndDisable();
}

}  // namespace webrtc
