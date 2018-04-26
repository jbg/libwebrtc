/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PACKETSOCKETFACTORY_H_
#define P2P_BASE_PACKETSOCKETFACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/asyncresolverinterface.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/proxyinfo.h"

namespace rtc {

// TODO(steveanton): Remove. Replaced by ClientTcpSocketCreateInfo.
// This structure contains options required to create TCP packet sockets.
struct PacketSocketTcpOptions {
  PacketSocketTcpOptions();
  ~PacketSocketTcpOptions();

  int opts = 0;
  std::vector<std::string> tls_alpn_protocols;
  std::vector<std::string> tls_elliptic_curves;
};

struct SocketCreateInfo {
  SocketAddress local_address;
  uint16_t min_port = 0;
  uint16_t max_port = 0;
};

struct UdpSocketCreateInfo : public SocketCreateInfo {};

struct TcpSocketCreateInfo : public SocketCreateInfo {
  int opts = 0;
};

struct ServerTcpSocketCreateInfo : public TcpSocketCreateInfo {};

struct ClientTcpSocketCreateInfo : public TcpSocketCreateInfo {
  ClientTcpSocketCreateInfo();
  ~ClientTcpSocketCreateInfo();

  SocketAddress remote_address;
  ProxyInfo proxy_info;
  std::string user_agent;
  std::vector<std::string> tls_alpn_protocols;
  std::vector<std::string> tls_elliptic_curves;
};

class PacketSocketFactory {
 public:
  enum Options {
    OPT_STUN = 0x04,

    // The TLS options below are mutually exclusive.
    OPT_TLS = 0x02,           // Real and secure TLS.
    OPT_TLS_FAKE = 0x01,      // Fake TLS with a dummy SSL handshake.
    OPT_TLS_INSECURE = 0x08,  // Insecure TLS without certificate validation.

    // Deprecated, use OPT_TLS_FAKE.
    OPT_SSLTCP = OPT_TLS_FAKE,
  };

  PacketSocketFactory() = default;
  virtual ~PacketSocketFactory() = default;

  // TODO(steveanton): Remove overload that returns a bare pointer.
  virtual AsyncPacketSocket* CreateUdpSocket(const SocketAddress& address,
                                             uint16_t min_port,
                                             uint16_t max_port);
  virtual std::unique_ptr<AsyncPacketSocket> CreateUdpSocket(
      const UdpSocketCreateInfo& create_info);

  // TODO(steveanton): Remove overload that returns a bare pointer.
  virtual AsyncPacketSocket* CreateServerTcpSocket(
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts);
  virtual std::unique_ptr<AsyncPacketSocket> CreateServerTcpSocket(
      const ServerTcpSocketCreateInfo& create_info);

  // TODO(steveanton): Remove overloads that returns a bare pointer.
  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const ProxyInfo& proxy_info,
      const std::string& user_agent,
      int opts);
  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const ProxyInfo& proxy_info,
      const std::string& user_agent,
      const PacketSocketTcpOptions& tcp_options);
  virtual std::unique_ptr<AsyncPacketSocket> CreateClientTcpSocket(
      const ClientTcpSocketCreateInfo& create_info);

  // TODO(steveanton): Remove overload that returns a bare pointer.
  virtual AsyncResolverInterface* CreateAsyncResolver();
  virtual std::unique_ptr<AsyncResolverInterface> CreateAsyncResolverUnique();

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(PacketSocketFactory);
};

}  // namespace rtc

#endif  // P2P_BASE_PACKETSOCKETFACTORY_H_
