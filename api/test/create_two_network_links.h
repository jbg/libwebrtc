/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_CREATE_TWO_NETWORK_LINKS_H_
#define API_TEST_CREATE_TWO_NETWORK_LINKS_H_

#include <utility>

#include "api/test/network_emulation_manager.h"

namespace webrtc {

std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
CreateTwoNetworkLinks(NetworkEmulationManager* emulation,
                      const BuiltInNetworkBehaviorConfig& config);

}  // namespace webrtc

#endif  // API_TEST_CREATE_TWO_NETWORK_LINKS_H_
