/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtls_transport.h"

#include <utility>

#include "absl/types/optional.h"
#include "api/dtls_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/sequence_checker.h"
#include "pc/ice_transport.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {
namespace {

absl::optional<DtlsTransportTlsRole> GetDtlsRole(
    cricket::DtlsTransportInternal* transport) {
  rtc::SSLRole ssl_role;
  if (transport->GetDtlsRole(&ssl_role)) {
    return ssl_role == rtc::SSL_CLIENT ? DtlsTransportTlsRole::kClient
                                       : DtlsTransportTlsRole::kServer;
  }
  return absl::nullopt;
}

absl::optional<int> GetTlsVersion(cricket::DtlsTransportInternal* transport) {
  int tls_version = 0;
  if (transport->GetSslVersionBytes(&tls_version))
    return tls_version;
  return absl::nullopt;
}

absl::optional<int> GetSslCipherSuite(
    cricket::DtlsTransportInternal* transport) {
  int ssl_cipher_suite = 0;
  if (transport->GetSslCipherSuite(&ssl_cipher_suite))
    return ssl_cipher_suite;
  return absl::nullopt;
}

absl::optional<int> GetSrtpCryptoSuite(
    cricket::DtlsTransportInternal* transport) {
  int srtp_cipher = 0;
  if (transport->GetSrtpCryptoSuite(&srtp_cipher))
    return srtp_cipher;
  return absl::nullopt;
}

}  // namespace

// Implementation of DtlsTransportInterface
DtlsTransport::DtlsTransport(
    std::unique_ptr<cricket::DtlsTransportInternal> internal)
    : owner_thread_(rtc::Thread::Current()),
      internal_dtls_transport_(std::move(internal)),
      ice_transport_(rtc::make_ref_counted<IceTransportWithPointer>(
          internal_dtls_transport_->ice_transport())),
      info_(internal_dtls_transport_->dtls_state(),
            GetDtlsRole(internal_dtls_transport_.get()),
            GetTlsVersion(internal_dtls_transport_.get()),
            GetSslCipherSuite(internal_dtls_transport_.get()),
            GetSrtpCryptoSuite(internal_dtls_transport_.get()),
            internal_dtls_transport_->GetRemoteSSLCertChain()) {
  internal_dtls_transport_->SubscribeDtlsTransportState(
      [this](cricket::DtlsTransportInternal* transport,
             DtlsTransportState state) {
        OnInternalDtlsState(transport, state);
      });
  internal_dtls_transport_->SubscribeDtlsRole([this](rtc::SSLRole role) {
    RTC_DCHECK_RUN_ON(owner_thread_);
    {
      MutexLock lock(&lock_);
      info_.set_role(role == rtc::SSL_CLIENT ? DtlsTransportTlsRole::kClient
                                             : DtlsTransportTlsRole::kServer);
    }
    if (observer_) {
      observer_->OnStateChange(Information());
    }
  });
}

DtlsTransport::~DtlsTransport() {
  // We depend on the signaling thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_dtls_transport_);
}

DtlsTransportInformation DtlsTransport::Information() {
  MutexLock lock(&lock_);
  return info_;
}

void DtlsTransport::RegisterObserver(DtlsTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  observer_ = observer;
}

void DtlsTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

rtc::scoped_refptr<IceTransportInterface> DtlsTransport::ice_transport() {
  return ice_transport_;
}

// Internal functions
void DtlsTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal());
  const DtlsTransportState state = internal()->dtls_state();
  bool must_send_event = (state != DtlsTransportState::kClosed);
  // The destructor of cricket::DtlsTransportInternal calls back
  // into DtlsTransport, so we can't hold the lock while releasing.
  std::unique_ptr<cricket::DtlsTransportInternal> transport_to_release;
  {
    MutexLock lock(&lock_);
    info_.set_state(DtlsTransportState::kClosed);
    transport_to_release = std::move(internal_dtls_transport_);
    ice_transport_->Clear();
  }
  if (observer_ && must_send_event) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::OnInternalDtlsState(
    cricket::DtlsTransportInternal* transport,
    DtlsTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(transport == internal());
  RTC_DCHECK_EQ(state, internal()->dtls_state());

  {
    MutexLock lock(&lock_);
    info_.set_state(state);
  }

  if (observer_) {
    observer_->OnStateChange(Information());
  }
}

}  // namespace webrtc
