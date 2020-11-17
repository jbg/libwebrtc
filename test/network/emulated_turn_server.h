/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_EMULATED_TURN_SERVER_H_
#define TEST_NETWORK_EMULATED_TURN_SERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/test/network_emulation_manager.h"
#include "api/transport/stun.h"
#include "p2p/base/turn_server.h"
#include "rtc_base/async_packet_socket.h"

namespace webrtc {
namespace test {

// Framework assumes that rtc::NetworkManager is called from network thread.
class EmulatedTURNServer : public EmulatedTURNServerInterface,
                           public cricket::TurnAuthInterface {
 public:
  EmulatedTURNServer(std::unique_ptr<rtc::Thread> thread,
                     EmulatedEndpoint* client,
                     EmulatedEndpoint* peer);
  ~EmulatedTURNServer() override;

  IceServerConfig GetIceServerConfig() override { return ice_config_; }

  // Get client endpoint.
  EmulatedEndpoint* GetClientEndpoint() override { return client_; }

  rtc::SocketAddress GetClientEndpointAddress() override {
    return client_address_;
  }

  // Get peer endpoint.
  EmulatedEndpoint* GetPeerEndpoint() override { return peer_; }

  // Wrap a EmulatedEndpoint in a AsyncPacketSocket to bridge interaction
  // with TurnServer;
  rtc::AsyncPacketSocket* Wrap(EmulatedEndpoint* endpoint);

  // cricket::TurnAuthInterface
  bool GetKey(const std::string& username,
              const std::string& realm,
              std::string* key) override {
    return cricket::ComputeStunCredentialHash(username, realm, username, key);
  }

  rtc::AsyncPacketSocket* CreatePeerSocket() { return Wrap(peer_); }

 private:
  std::unique_ptr<rtc::Thread> thread_;
  rtc::SocketAddress client_address_;
  IceServerConfig ice_config_;
  EmulatedEndpoint* client_;
  EmulatedEndpoint* peer_;
  std::unique_ptr<cricket::TurnServer> turn_server_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_EMULATED_TURN_SERVER_H_
