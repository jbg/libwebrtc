/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/network.h"
#include "api/test/network_emulation_manager.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

class SocketReader : public sigslot::has_slots<> {
 public:
  explicit SocketReader(rtc::AsyncSocket* socket) : socket_(socket) {
    socket_->SignalReadEvent.connect(this, &SocketReader::OnReadEvent);
    size_ = 128 * 1024;
    buf_ = new char[size_];
  }
  ~SocketReader() override { delete[] buf_; }

  void OnReadEvent(rtc::AsyncSocket* socket) {
    RTC_DCHECK(socket_ == socket);
    int64_t timestamp;
    len_ = socket_->Recv(buf_, size_, &timestamp);
    {
      rtc::CritScope crit(&lock_);
      received_count_++;
    }
  }

  int ReceivedCount() {
    rtc::CritScope crit(&lock_);
    return received_count_;
  }

 private:
  rtc::AsyncSocket* socket_;
  char* buf_;
  size_t size_;
  int len_;

  rtc::CriticalSection lock_;
  int received_count_ RTC_GUARDED_BY(lock_) = 0;
};

TEST(NetworkEmulationManager, Run) {
  std::unique_ptr<NetworkEmulationManager> network_manager =
      CreateNetworkEmulationManager(webrtc::Clock::GetRealTimeClock());

  NetworkNode* alice_node = network_manager->CreateTransparentNode();
  NetworkNode* bob_node = network_manager->CreateTransparentNode();
  EndpointNode* alice_endpoint =
      network_manager->CreateEndpoint(alice_node, bob_node);
  EndpointNode* bob_endpoint =
      network_manager->CreateEndpoint(bob_node, alice_node);
  network_manager->CreateRoute(alice_endpoint, bob_endpoint);
  network_manager->CreateRoute(bob_endpoint, alice_endpoint);

  auto* nt1 = network_manager->CreateNetworkThread({alice_endpoint});
  auto* nt2 = network_manager->CreateNetworkThread({bob_endpoint});

  network_manager->Start();

  for (uint64_t j = 0; j < 2; j++) {
    auto* s1 = nt1->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
    auto* s2 = nt2->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);

    SocketReader r1(s1);
    SocketReader r2(s2);

    rtc::SocketAddress a1("127.0.0.1", 81);
    rtc::SocketAddress a2("127.0.0.1", 82);

    s1->Bind(a1);
    s2->Bind(a2);

    s1->Connect(a2);
    s2->Connect(a1);

    rtc::CopyOnWriteBuffer data("Hello");
    for (uint64_t i = 0; i < 1000; i++) {
      s1->Send(data.data(), data.size());
      s2->Send(data.data(), data.size());
    }

    rtc::Event wait;
    wait.Wait(1000);
    ASSERT_EQ(r1.ReceivedCount(), 1000);
    ASSERT_EQ(r2.ReceivedCount(), 1000);

    delete s1;
    delete s2;
  }

  network_manager->Stop();
}

}  // namespace test
}  // namespace webrtc
