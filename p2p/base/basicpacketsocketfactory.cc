/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basicpacketsocketfactory.h"

#include <memory>
#include <utility>

#include "p2p/base/asyncstuntcpsocket.h"
#include "p2p/base/stun.h"
#include "rtc_base/asynctcpsocket.h"
#include "rtc_base/asyncudpsocket.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/socketadapters.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"

namespace rtc {

BasicPacketSocketFactory::BasicPacketSocketFactory()
    : thread_(Thread::Current()) {}

BasicPacketSocketFactory::BasicPacketSocketFactory(Thread* thread)
    : thread_(thread) {}

BasicPacketSocketFactory::BasicPacketSocketFactory(
    SocketFactory* socket_factory)
    : socket_factory_(socket_factory) {}

BasicPacketSocketFactory::~BasicPacketSocketFactory() = default;

std::unique_ptr<AsyncPacketSocket> BasicPacketSocketFactory::CreateUdpSocket(
    const UdpSocketCreateInfo& create_info) {
  // UDP sockets are simple.
  std::unique_ptr<AsyncSocket> socket(socket_factory()->CreateAsyncSocket(
      create_info.local_address.family(), SOCK_DGRAM));
  if (!socket) {
    return nullptr;
  }
  if (BindSocket(socket.get(), create_info) < 0) {
    RTC_LOG(LS_ERROR) << "UDP bind failed with error " << socket->GetError();
    return nullptr;
  }
  return rtc::MakeUnique<AsyncUDPSocket>(socket.release());
}

std::unique_ptr<AsyncPacketSocket>
BasicPacketSocketFactory::CreateServerTcpSocket(
    const ServerTcpSocketCreateInfo& create_info) {
  // Fail if TLS is required.
  if (create_info.opts & PacketSocketFactory::OPT_TLS) {
    RTC_LOG(LS_ERROR) << "TLS support currently is not available.";
    return nullptr;
  }

  std::unique_ptr<AsyncSocket> socket(socket_factory()->CreateAsyncSocket(
      create_info.local_address.family(), SOCK_STREAM));
  if (!socket) {
    return nullptr;
  }

  if (BindSocket(socket.get(), create_info) < 0) {
    RTC_LOG(LS_ERROR) << "TCP bind failed with error " << socket->GetError();
    return nullptr;
  }

  // If using fake TLS, wrap the TCP socket in a pseudo-SSL socket.
  if (create_info.opts & PacketSocketFactory::OPT_TLS_FAKE) {
    RTC_DCHECK(!(create_info.opts & PacketSocketFactory::OPT_TLS));
    socket = rtc::MakeUnique<AsyncSSLSocket>(socket.release());
  }

  // Set TCP_NODELAY (via OPT_NODELAY) for improved performance.
  // See http://go/gtalktcpnodelayexperiment
  socket->SetOption(Socket::OPT_NODELAY, 1);

  if (create_info.opts & PacketSocketFactory::OPT_STUN) {
    return rtc::MakeUnique<cricket::AsyncStunTCPSocket>(socket.release(), true);
  }

  return rtc::MakeUnique<AsyncTCPSocket>(socket.release(), true);
}

std::unique_ptr<AsyncPacketSocket>
BasicPacketSocketFactory::CreateClientTcpSocket(
    const ClientTcpSocketCreateInfo& create_info) {
  std::unique_ptr<AsyncSocket> socket(socket_factory()->CreateAsyncSocket(
      create_info.local_address.family(), SOCK_STREAM));
  if (!socket) {
    return nullptr;
  }

  if (BindSocket(socket.get(), create_info) < 0) {
    // Allow BindSocket to fail if we're binding to the ANY address, since this
    // is mostly redundant in the first place. The socket will be bound when we
    // call Connect() instead.
    if (create_info.local_address.IsAnyIP()) {
      RTC_LOG(LS_WARNING) << "TCP bind failed with error " << socket->GetError()
                          << "; ignoring since socket is using 'any' address.";
    } else {
      RTC_LOG(LS_ERROR) << "TCP bind failed with error " << socket->GetError();
      return nullptr;
    }
  }

  // If using a proxy, wrap the socket in a proxy socket.
  if (create_info.proxy_info.type == PROXY_SOCKS5) {
    socket = rtc::MakeUnique<AsyncSocksProxySocket>(
        socket.release(), create_info.proxy_info.address,
        create_info.proxy_info.username, create_info.proxy_info.password);
  } else if (create_info.proxy_info.type == PROXY_HTTPS) {
    socket = rtc::MakeUnique<AsyncHttpsProxySocket>(
        socket.release(), create_info.user_agent,
        create_info.proxy_info.address, create_info.proxy_info.username,
        create_info.proxy_info.password);
  }

  // Assert that at most one TLS option is used.
  int tlsOpts = create_info.opts & (PacketSocketFactory::OPT_TLS |
                                    PacketSocketFactory::OPT_TLS_FAKE |
                                    PacketSocketFactory::OPT_TLS_INSECURE);
  RTC_DCHECK_EQ((tlsOpts & (tlsOpts - 1)), 0);

  if ((tlsOpts & PacketSocketFactory::OPT_TLS) ||
      (tlsOpts & PacketSocketFactory::OPT_TLS_INSECURE)) {
    // Using TLS, wrap the socket in an SSL adapter.
    std::unique_ptr<SSLAdapter> ssl_adapter(
        SSLAdapter::Create(socket.release()));
    if (!ssl_adapter) {
      return nullptr;
    }

    if (tlsOpts & PacketSocketFactory::OPT_TLS_INSECURE) {
      ssl_adapter->SetIgnoreBadCert(true);
    }

    ssl_adapter->SetAlpnProtocols(create_info.tls_alpn_protocols);
    ssl_adapter->SetEllipticCurves(create_info.tls_elliptic_curves);

    if (ssl_adapter->StartSSL(create_info.remote_address.hostname().c_str(),
                              false) != 0) {
      RTC_LOG(LS_INFO) << create_info.remote_address.hostname();
      return nullptr;
    }

    socket = std::move(ssl_adapter);
  } else if (tlsOpts & PacketSocketFactory::OPT_TLS_FAKE) {
    // Using fake TLS, wrap the TCP socket in a pseudo-SSL socket.
    socket = rtc::MakeUnique<AsyncSSLSocket>(socket.release());
  }

  if (socket->Connect(create_info.remote_address) < 0) {
    RTC_LOG(LS_ERROR) << "TCP connect failed with error " << socket->GetError();
    return nullptr;
  }

  // Finally, wrap that socket in a TCP or STUN TCP packet socket.
  std::unique_ptr<AsyncPacketSocket> tcp_socket;
  if (create_info.opts & PacketSocketFactory::OPT_STUN) {
    tcp_socket =
        rtc::MakeUnique<cricket::AsyncStunTCPSocket>(socket.release(), false);
  } else {
    tcp_socket = rtc::MakeUnique<AsyncTCPSocket>(socket.release(), false);
  }

  // Set TCP_NODELAY (via OPT_NODELAY) for improved performance.
  // See http://go/gtalktcpnodelayexperiment
  tcp_socket->SetOption(Socket::OPT_NODELAY, 1);

  return tcp_socket;
}

std::unique_ptr<AsyncResolverInterface>
BasicPacketSocketFactory::CreateAsyncResolverUnique() {
  return rtc::MakeUnique<AsyncResolver>();
}

int BasicPacketSocketFactory::BindSocket(AsyncSocket* socket,
                                         const SocketCreateInfo& create_info) {
  int ret = -1;
  if (create_info.min_port == 0 && create_info.max_port == 0) {
    // If there's no port range, let the OS pick a port for us.
    ret = socket->Bind(create_info.local_address);
  } else {
    // Otherwise, try to find a port in the provided range.
    for (int port = create_info.min_port;
         ret < 0 && port <= create_info.max_port; ++port) {
      ret =
          socket->Bind(SocketAddress(create_info.local_address.ipaddr(), port));
    }
  }
  return ret;
}

SocketFactory* BasicPacketSocketFactory::socket_factory() {
  if (thread_) {
    RTC_DCHECK_RUN_ON(thread_);
    return thread_->socketserver();
  } else {
    return socket_factory_;
  }
}

}  // namespace rtc
