/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_MANAGER_INTERFACE_H_
#define API_TEST_NETWORK_EMULATION_MANAGER_INTERFACE_H_

#include <memory>
#include <vector>

#include "api/test/simulated_network.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {

// This API is still in development and can be changed without prior notice.

struct EndpointConfig {
  enum IpAddressFamily { kIpv4, kIpv6 };

  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;
  // If specified will be used as IP address for endpoint node. Should be unique
  // among all created nodes.
  absl::optional<rtc::IPAddress> ip;
};

class EmulatedNetworkNodeInterface {
 public:
  virtual ~EmulatedNetworkNodeInterface() = default;
};

class EndpointNodeInterface {
 public:
  virtual ~EndpointNodeInterface() = default;

  virtual rtc::IPAddress GetPeerLocalAddress() const = 0;
};

class NetworkEmulationManagerInterface {
 public:
  virtual ~NetworkEmulationManagerInterface() = default;

  virtual EmulatedNetworkNodeInterface* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;

  virtual EndpointNodeInterface* CreateEndpoint(EndpointConfig config) = 0;

  virtual void CreateRoute(EndpointNodeInterface* from,
                           std::vector<EmulatedNetworkNodeInterface*> via_nodes,
                           EndpointNodeInterface* to) = 0;
  virtual void ClearRoute(EndpointNodeInterface* from,
                          std::vector<EmulatedNetworkNodeInterface*> via_nodes,
                          EndpointNodeInterface* to) = 0;

  virtual rtc::Thread* CreateNetworkThread(
      std::vector<EndpointNodeInterface*> endpoints) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_MANAGER_INTERFACE_H_
