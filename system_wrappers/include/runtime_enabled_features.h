/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SYSTEM_WRAPPERS_INCLUDE_RUNTIME_ENABLED_FEATURES_H_
#define SYSTEM_WRAPPERS_INCLUDE_RUNTIME_ENABLED_FEATURES_H_

// These functions for querying enabled runtime features must be implemented
// by all webrtc clients (such as Chrome).
// Default implementation is provided in:
//
//    system_wrappers/system_wrappers:runtime_enabled_features_default

namespace webrtc {
namespace runtime_enabled_features {
bool IsWebRtcDualStreamModeEnabled();

}  // namespace runtime_enabled_features
}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_RUNTIME_ENABLED_FEATURES_H_
