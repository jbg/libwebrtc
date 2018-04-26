/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "p2p/base/packetsocketfactory.h"

#include "rtc_base/ptr_util.h"

namespace rtc {

PacketSocketTcpOptions::PacketSocketTcpOptions() = default;
PacketSocketTcpOptions::~PacketSocketTcpOptions() = default;

ClientTcpSocketCreateInfo::ClientTcpSocketCreateInfo() = default;
ClientTcpSocketCreateInfo::~ClientTcpSocketCreateInfo() = default;

AsyncPacketSocket* PacketSocketFactory::CreateUdpSocket(
    const SocketAddress& address,
    uint16_t min_port,
    uint16_t max_port) {
  UdpSocketCreateInfo create_info;
  create_info.local_address = address;
  create_info.min_port = min_port;
  create_info.max_port = max_port;
  return CreateUdpSocket(create_info).release();
}

std::unique_ptr<AsyncPacketSocket> PacketSocketFactory::CreateUdpSocket(
    const UdpSocketCreateInfo& create_info) {
  return rtc::WrapUnique(CreateUdpSocket(
      create_info.local_address, create_info.min_port, create_info.max_port));
}

AsyncPacketSocket* PacketSocketFactory::CreateServerTcpSocket(
    const SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  ServerTcpSocketCreateInfo create_info;
  create_info.local_address = local_address;
  create_info.min_port = min_port;
  create_info.max_port = max_port;
  create_info.opts = opts;
  return CreateServerTcpSocket(create_info).release();
}

std::unique_ptr<AsyncPacketSocket> PacketSocketFactory::CreateServerTcpSocket(
    const ServerTcpSocketCreateInfo& create_info) {
  return rtc::WrapUnique(
      CreateServerTcpSocket(create_info.local_address, create_info.min_port,
                            create_info.max_port, create_info.opts));
}

AsyncPacketSocket* PacketSocketFactory::CreateClientTcpSocket(
    const SocketAddress& local_address,
    const SocketAddress& remote_address,
    const ProxyInfo& proxy_info,
    const std::string& user_agent,
    int opts) {
  PacketSocketTcpOptions tcp_options;
  tcp_options.opts = opts;
  return CreateClientTcpSocket(local_address, remote_address, proxy_info,
                               user_agent, tcp_options);
}

AsyncPacketSocket* PacketSocketFactory::CreateClientTcpSocket(
    const SocketAddress& local_address,
    const SocketAddress& remote_address,
    const ProxyInfo& proxy_info,
    const std::string& user_agent,
    const PacketSocketTcpOptions& tcp_options) {
  ClientTcpSocketCreateInfo create_info;
  create_info.local_address = local_address;
  create_info.opts = tcp_options.opts;
  create_info.remote_address = remote_address;
  create_info.proxy_info = proxy_info;
  create_info.user_agent = user_agent;
  create_info.tls_alpn_protocols = tcp_options.tls_alpn_protocols;
  create_info.tls_elliptic_curves = tcp_options.tls_elliptic_curves;
  return CreateClientTcpSocket(create_info).release();
}

std::unique_ptr<AsyncPacketSocket> PacketSocketFactory::CreateClientTcpSocket(
    const ClientTcpSocketCreateInfo& create_info) {
  if (create_info.tls_alpn_protocols.empty() &&
      create_info.tls_elliptic_curves.empty()) {
    return rtc::WrapUnique(CreateClientTcpSocket(
        create_info.local_address, create_info.remote_address,
        create_info.proxy_info, create_info.user_agent, create_info.opts));
  } else {
    PacketSocketTcpOptions tcp_options;
    tcp_options.opts = create_info.opts;
    tcp_options.tls_alpn_protocols = create_info.tls_alpn_protocols;
    tcp_options.tls_elliptic_curves = create_info.tls_elliptic_curves;
    return rtc::WrapUnique(CreateClientTcpSocket(
        create_info.local_address, create_info.remote_address,
        create_info.proxy_info, create_info.user_agent, tcp_options));
  }
}

AsyncResolverInterface* PacketSocketFactory::CreateAsyncResolver() {
  return CreateAsyncResolverUnique().release();
}

std::unique_ptr<AsyncResolverInterface>
PacketSocketFactory::CreateAsyncResolverUnique() {
  return rtc::WrapUnique(CreateAsyncResolver());
}

}  // namespace rtc
