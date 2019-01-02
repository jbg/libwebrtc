/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_MANAGER_H_
#define API_TEST_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <vector>

#include "api/test/network.h"
#include "api/test/simulated_network.h"
#include "rtc_base/thread.h"

namespace webrtc {

// This API is in development. It can be changed/removed without notice.
class NetworkEmulationManager {
 public:
  virtual ~NetworkEmulationManager() = default;

  // Creates simple network node, that proxy all data without any delays and
  // drops.
  virtual NetworkNode* CreateTransparentNode() = 0;
  // Create network node, that uses provided |network_behavior| to emulate
  // network conditions.
  virtual NetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;
  // Register externally created network node. Will take ownership of that node.
  virtual NetworkNode* RegisterNode(std::unique_ptr<NetworkNode> node) = 0;

  // Creates endpoint node.
  virtual EndpointNode* CreateEndpoint(NetworkNode* entry_node,
                                       NetworkNode* exit_node) = 0;

  // Creates route between two endpoints without intermediate nodes.
  virtual void CreateRoute(EndpointNode* from, EndpointNode* to) = 0;
  virtual void CreateRoute(EndpointNode* from,
                           std::vector<NetworkNode*> via_nodes,
                           EndpointNode* to) = 0;

  // Creates network thread, which will use provided endpoints for network
  // emulation.
  virtual rtc::Thread* CreateNetworkThread(
      std::vector<EndpointNode*> endpoints) = 0;

  // Starts network emulation manager. No new entities can be created after
  // start and before stop.
  virtual void Start() = 0;
  // Stops network emulation manager. After manager can be started again. All
  // owned entities will live until destruction.
  virtual void Stop() = 0;

 protected:
  // Helper methods to access EndpointNode protected.
  NetworkNode* GetEntryNode(EndpointNode* node) { return node->GetEntryNode(); }
  NetworkNode* GetExitNode(EndpointNode* node) { return node->GetExitNode(); }
  void SetConnectedEndpoint(EndpointNode* n1, EndpointNode* n2) {
    n1->SetConnectedEndpoint(n2);
  }
  void SetNetworkThread(EndpointNode* n, rtc::Thread* t) {
    n->SetNetworkThread(t);
  }
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_MANAGER_H_
