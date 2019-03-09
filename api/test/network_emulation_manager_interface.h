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
#include "rtc_base/network.h"
#include "rtc_base/thread.h"

namespace webrtc {

// This API is still in development and can be changed without prior notice.
class EmulatedNetworkNode;
class EmulatedEndpoint;
class EmulatedRoute;

struct EmulatedEndpointConfig {
  enum IpAddressFamily { kIpv4, kIpv6 };

  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;
  // If specified will be used as IP address for endpoint node. Should be unique
  // among all created nodes.
  absl::optional<rtc::IPAddress> ip;
};

class NetworkEmulationManagerInterface {
 public:
  virtual ~NetworkEmulationManagerInterface() = default;

  virtual EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;

  virtual EmulatedEndpoint* CreateEndpoint(EmulatedEndpointConfig config) = 0;

  // Creates a route between endpoints going through specified network nodes.
  // Return object can be used to remove created route.
  //
  // Second attempt of creating route between same endpoints will fail.
  virtual EmulatedRoute* CreateRoute(
      EmulatedEndpoint* from,
      std::vector<EmulatedNetworkNode*> via_nodes,
      EmulatedEndpoint* to) = 0;
  // Removes route previously created by CreateRoute(...).
  // Attempt to remove previously removed route will fail.
  // If route which is created not by CreateRoute(...) call will be passed
  // behavior is unspecified.
  virtual void ClearRoute(EmulatedRoute* route) = 0;

  virtual rtc::Thread* CreateNetworkThread(
      std::vector<EmulatedEndpoint*> endpoints) = 0;
  virtual rtc::NetworkManager* CreateNetworkManager(
      std::vector<EmulatedEndpoint*> endpoints) = 0;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_MANAGER_INTERFACE_H_
