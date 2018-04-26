/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_BASICPACKETSOCKETFACTORY_H_
#define P2P_BASE_BASICPACKETSOCKETFACTORY_H_

#include <memory>

#include "p2p/base/packetsocketfactory.h"

namespace rtc {

class AsyncSocket;
class SocketFactory;
class Thread;

class BasicPacketSocketFactory final : public PacketSocketFactory {
 public:
  BasicPacketSocketFactory();
  explicit BasicPacketSocketFactory(Thread* thread);
  explicit BasicPacketSocketFactory(SocketFactory* socket_factory);
  ~BasicPacketSocketFactory() override;

  // PacketSocketFactory implementation.
  using PacketSocketFactory::CreateUdpSocket;
  std::unique_ptr<AsyncPacketSocket> CreateUdpSocket(
      const UdpSocketCreateInfo& create_info) override;
  using PacketSocketFactory::CreateServerTcpSocket;
  std::unique_ptr<AsyncPacketSocket> CreateServerTcpSocket(
      const ServerTcpSocketCreateInfo& create_info) override;
  using PacketSocketFactory::CreateClientTcpSocket;
  std::unique_ptr<AsyncPacketSocket> CreateClientTcpSocket(
      const ClientTcpSocketCreateInfo& create_info) override;
  std::unique_ptr<AsyncResolverInterface> CreateAsyncResolverUnique() override;

 private:
  int BindSocket(AsyncSocket* socket, const SocketCreateInfo& create_info);

  SocketFactory* socket_factory();

  Thread* const thread_ = nullptr;
  SocketFactory* const socket_factory_ = nullptr;
};

}  // namespace rtc

#endif  // P2P_BASE_BASICPACKETSOCKETFACTORY_H_
