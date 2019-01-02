/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_network_emulation_manager.h"

#include "absl/memory/memory.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/network_emulation_manager.h"

namespace webrtc {

std::unique_ptr<NetworkEmulationManager> CreateNetworkEmulationManager(
    webrtc::Clock* clock) {
  return absl::make_unique<test::NetworkEmulationManagerImpl>(clock);
}

}  // namespace webrtc
