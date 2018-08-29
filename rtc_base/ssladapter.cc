/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/ssladapter.h"

#include "rtc_base/openssladapter.h"

///////////////////////////////////////////////////////////////////////////////

namespace rtc {

SSLConfig::SSLConfig()
    : SSLConfig(true /* enable_ocsp_stapling */,
                true /* enable_signed_cert_timestamp */,
                false /* enable_tls_channel_id */,
                false /* enable_grease */,
                TlsCertPolicy::TLS_CERT_POLICY_SECURE,
                absl::nullopt /* max_ssl_version */,
                absl::nullopt /* tls_alpn_protocols */,
                absl::nullopt /* tls_elliptic_curves */) {}

SSLConfig::SSLConfig(
    bool enable_ocsp_stapling,
    bool enable_signed_cert_timestamp,
    bool enable_tls_channel_id,
    bool enable_grease,
    TlsCertPolicy tls_cert_policy,
    absl::optional<int> max_ssl_version,
    absl::optional<std::vector<std::string>> tls_alpn_protocols,
    absl::optional<std::vector<std::string>> tls_elliptic_curves)
    : enable_ocsp_stapling(enable_ocsp_stapling),
      enable_signed_cert_timestamp(enable_signed_cert_timestamp),
      enable_tls_channel_id(enable_tls_channel_id),
      enable_grease(enable_grease),
      tls_cert_policy(tls_cert_policy),
      max_ssl_version(max_ssl_version),
      tls_alpn_protocols(tls_alpn_protocols),
      tls_elliptic_curves(tls_elliptic_curves) {}

SSLConfig::SSLConfig(const SSLConfig&) = default;
SSLConfig::~SSLConfig() = default;

///////////////////////////////////////////////////////////////////////////////

SSLAdapterFactory* SSLAdapterFactory::Create() {
  return new OpenSSLAdapterFactory();
}

SSLAdapter* SSLAdapter::Create(AsyncSocket* socket) {
  return new OpenSSLAdapter(socket);
}

///////////////////////////////////////////////////////////////////////////////

bool InitializeSSL() {
  return OpenSSLAdapter::InitializeSSL();
}

bool CleanupSSL() {
  return OpenSSLAdapter::CleanupSSL();
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
