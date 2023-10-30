/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FILL_DEFAULT_MEDIA_DEPENDENCIES_H_
#define API_FILL_DEFAULT_MEDIA_DEPENDENCIES_H_

#include "api/peer_connection_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Fills required but missing media related dependencies with default factories.
RTC_EXPORT void FillDefaultMediaDependencies(
    PeerConnectionFactoryDependencies& deps);

}  // namespace webrtc

#endif  // API_FILL_DEFAULT_MEDIA_DEPENDENCIES_H_
