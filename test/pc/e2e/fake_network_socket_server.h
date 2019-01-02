/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_FAKE_NETWORK_SOCKET_SERVER_H_
#define TEST_PC_E2E_FAKE_NETWORK_SOCKET_SERVER_H_

#include <atomic>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "api/test/network.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/event.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/socket.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/socketserver.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

// FakeNetworkSocketServer must outlive any sockets it creates.
class FakeNetworkSocketServer : public rtc::SocketServer,
                                public sigslot::has_slots<> {
 public:
  FakeNetworkSocketServer(int socket_id_init_value,
                          webrtc::Clock* clock,
                          std::vector<webrtc::EndpointNode*> endpoints);
  ~FakeNetworkSocketServer() override;

  void OnMessageQueueDestroyed();
  webrtc::EndpointNode* GetEndpointNode(const rtc::SocketAddress& addr);

  // rtc::SocketFactory methods:
  rtc::Socket* CreateSocket(int family, int type) override;
  rtc::AsyncSocket* CreateAsyncSocket(int family, int type) override;

  // rtc::SocketServer methods:
  // Called by the network thread when this server is installed, kicking off the
  // message handler loop.
  void SetMessageQueue(rtc::MessageQueue* msg_queue) override;
  bool Wait(int cms, bool process_io) override;
  void WakeUp() override;

 private:
  std::atomic_int next_socket_id_;
  webrtc::Clock* const clock_;
  std::vector<webrtc::EndpointNode*> endpoints_;
  rtc::Event wakeup_;
  rtc::MessageQueue* msg_queue_;
};

class FakeNetworkSocket : public webrtc::FakeNetworkSocket {
 public:
  FakeNetworkSocket(std::string id,
                    webrtc::Clock* clock,
                    FakeNetworkSocketServer* socket_server);
  ~FakeNetworkSocket() override;

  void DeliverPacket(rtc::CopyOnWriteBuffer packet,
                     const rtc::SocketAddress& source_addr) override;

  // rtc::Socket methods:
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Bind(const rtc::SocketAddress& addr) override;
  int Connect(const rtc::SocketAddress& addr) override;
  int Close() override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override;
  int Listen(int backlog) override;
  rtc::AsyncSocket* Accept(rtc::SocketAddress* paddr) override;
  int GetError() const override;
  void SetError(int error) override;
  ConnState GetState() const override;
  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

 private:
  // Binds this socket to the default address in the same family as |addr|.
  int BindDefault(const rtc::SocketAddress& addr);

  struct Packet {
    Packet();
    Packet(Packet&& packet);
    ~Packet();

    rtc::SocketAddress source_addr;
    std::vector<uint8_t> data;
  };

  webrtc::Clock* const clock_;
  FakeNetworkSocketServer* const socket_server_;
  EndpointNode* endpoint_;

  rtc::SocketAddress local_addr_;
  rtc::SocketAddress remote_addr_;
  ConnState state_;
  int error_;
  std::deque<Packet> packet_queue_;
  std::map<Option, int> options_map_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_FAKE_NETWORK_SOCKET_SERVER_H_
