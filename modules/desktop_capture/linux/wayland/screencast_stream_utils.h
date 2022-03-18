/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <vector>

namespace webrtc {

// Locks pw_thread_loop in the current scope
class PipeWireThreadLoopLock {
 public:
  explicit PipeWireThreadLoopLock(pw_thread_loop* loop);
  ~PipeWireThreadLoopLock();

 private:
  pw_thread_loop* loop_;
};

struct PipeWireVersion {
  int major = 0;
  int minor = 0;
  int micro = 0;
};

// Returns whether current version is newer or same as required version
bool operator>=(const PipeWireVersion& current_pw_version,
                const PipeWireVersion& required_pw_version);
// Returns whether current version is older or same as required version
bool operator<=(const PipeWireVersion& current_pw_version,
                const PipeWireVersion& required_pw_version);

// Parses PipeWire version from a string in format "x.y.z"
PipeWireVersion ParsePipeWireVersion(const char* version);

// Returns a spa_pod used to build PipeWire stream format using given
// arguments. Modifiers are optional value and when present they will be
// used with SPA_POD_PROP_FLAG_MANDATORY and SPA_POD_PROP_FLAG_DONT_FIXATE
// flags.
spa_pod* BuildFormat(spa_pod_builder* builder,
                     uint32_t format,
                     const std::vector<uint64_t>& modifiers,
                     struct spa_rectangle* resolution);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
