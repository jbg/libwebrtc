/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSLADAPTER_H_
#define RTC_BASE_SSLADAPTER_H_

#include <string>
#include <vector>

#include "rtc_base/asyncsocket.h"
#include "rtc_base/sslcertificate.h"
#include "rtc_base/sslstreamadapter.h"

namespace rtc {

class SSLAdapter;

// Class for creating SSL adapters with shared state, e.g., a session cache,
// which allows clients to resume SSL sessions to previously-contacted hosts.
// Clients should create the factory using Create(), set up the factory as
// needed using SetMode, and then call CreateAdapter to create adapters when
// needed.
class SSLAdapterFactory {
 public:
  virtual ~SSLAdapterFactory() {}

  // Specifies whether TLS or DTLS is to be used for the SSL adapters.
  virtual void SetMode(SSLMode mode) = 0;

  // Specify a custom certificate verifier for SSL.
  virtual void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) = 0;

  // Creates a new SSL adapter, but from a shared context.
  virtual SSLAdapter* CreateAdapter(AsyncSocket* socket) = 0;

  static SSLAdapterFactory* Create();
};

// Class that abstracts a client-to-server SSL session. It can be created
// standalone, via SSLAdapter::Create, or through a factory as described above,
// in which case it will share state with other SSLAdapters created from the
// same factory.
// After creation, call StartSSL to initiate the SSL handshake to the server.
class SSLAdapter : public AsyncSocketAdapter {
 public:
  explicit SSLAdapter(AsyncSocket* socket) : AsyncSocketAdapter(socket) {}

  // Methods that control server certificate verification, used in unit tests.
  // Do not call these methods in production code.
  // TODO(juberti): Remove the opportunistic encryption mechanism in
  // BasicPacketSocketFactory that uses this function.
  virtual void SetIgnoreBadCert(bool ignore) = 0;

  // Indicates whether to enable OCSP stapling in TLS.
  virtual void SetEnableOcspStapling(bool enable_ocsp_stapling) = 0;
  // Indicates whether to enable the signed certificate timestamp extension in
  // TLS.
  virtual void SetEnableSignedCertTimestamp(
      bool enable_signed_cert_timestamp) = 0;
  // Indicates whether to enable the TLS Channel ID extension.
  virtual void SetEnableTlsChannelId(bool enable_tls_channel_id) = 0;
  // Indicates whether to enable the TLS GREASE extension.
  virtual void SetEnableGrease(bool enable_grease) = 0;
  // Highest supported SSL version, as defined in the supported_versions TLS
  // extension.
  virtual void SetMaxSslVersion(const absl::optional<int>& max_ssl_version) = 0;
  // List of protocols to be used in the TLS ALPN extension.
  virtual void SetAlpnProtocols(
      const absl::optional<std::vector<std::string>>& tls_alpn_protocols) = 0;
  // List of elliptic curves to be used in the TLS elliptic curves extension.
  // Only curve names supported by OpenSSL should be used (eg.
  // "P-256","X25519").
  virtual void SetEllipticCurves(
      const absl::optional<std::vector<std::string>>& tls_elliptic_curves) = 0;

  // Do DTLS or TLS (default is TLS, if unspecified)
  virtual void SetMode(SSLMode mode) = 0;
  // Specify a custom certificate verifier for SSL.
  virtual void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) = 0;

  // Set the certificate this socket will present to incoming clients.
  virtual void SetIdentity(SSLIdentity* identity) = 0;

  // Choose whether the socket acts as a server socket or client socket.
  virtual void SetRole(SSLRole role) = 0;

  // StartSSL returns 0 if successful.
  // If StartSSL is called while the socket is closed or connecting, the SSL
  // negotiation will begin as soon as the socket connects.
  // TODO(juberti): Remove |restartable|.
  virtual int StartSSL(const char* hostname, bool restartable = false) = 0;

  // When an SSLAdapterFactory is used, an SSLAdapter may be used to resume
  // a previous SSL session, which results in an abbreviated handshake.
  // This method, if called after SSL has been established for this adapter,
  // indicates whether the current session is a resumption of a previous
  // session.
  virtual bool IsResumedSession() = 0;

  // Create the default SSL adapter for this platform. On failure, returns null
  // and deletes |socket|. Otherwise, the returned SSLAdapter takes ownership
  // of |socket|.
  static SSLAdapter* Create(AsyncSocket* socket);
};

///////////////////////////////////////////////////////////////////////////////

// Call this on the main thread, before using SSL.
// Call CleanupSSL when finished with SSL.
bool InitializeSSL();

// Call to cleanup additional threads, and also the main thread.
bool CleanupSSL();

}  // namespace rtc

#endif  // RTC_BASE_SSLADAPTER_H_
