/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtlssrtptransport.h"

#include <memory>
#include <string>
#include <utility>

#include "media/base/rtputils.h"
#include "rtc_base/sslstreamadapter.h"

namespace {
// Value specified in RFC 5764.
static const char kDtlsSrtpExporterLabel[] = "EXTRACTOR-dtls_srtp";
}  // namespace

namespace webrtc {

DtlsSrtpTransport::DtlsSrtpTransport(
    std::unique_ptr<webrtc::SrtpTransport> transport)
    : srtp_transport_(std::move(transport)) {
  RTC_DCHECK(srtp_transport_);
  ConnectToSrtpTransport();
}

void DtlsSrtpTransport::ConnectToSrtpTransport() {
  srtp_transport_->SignalPacketReceived.connect(
      this, &DtlsSrtpTransport::OnPacketReceived);
  srtp_transport_->SignalReadyToSend.connect(this,
                                             &DtlsSrtpTransport::OnReadyToSend);
}

void DtlsSrtpTransport::OnPacketReceived(bool rtcp,
                                         rtc::CopyOnWriteBuffer* packet,
                                         const rtc::PacketTime& packet_time) {
  SignalPacketReceived(rtcp, packet, packet_time);
}

void DtlsSrtpTransport::OnReadyToSend(bool ready) {
  SignalReadyToSend(ready);
}

bool DtlsSrtpTransport::SetupDtlsSrtp(bool rtcp) {
  bool ret = false;

  cricket::DtlsTransportInternal* transport =
      rtcp ? rtcp_dtls_transport_ : rtp_dtls_transport_;
  RTC_DCHECK(transport);
  RTC_DCHECK(transport->IsDtlsActive());

  int selected_crypto_suite;

  if (!transport->GetSrtpCryptoSuite(&selected_crypto_suite)) {
    LOG(LS_ERROR) << "No DTLS-SRTP selected crypto suite";
    return false;
  }

  LOG(LS_INFO) << "Installing keys from DTLS-SRTP on "
               << cricket::RtpRtcpStringLiteral(rtcp);

  int key_len;
  int salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(selected_crypto_suite, &key_len,
                                     &salt_len)) {
    LOG(LS_ERROR) << "Unknown DTLS-SRTP crypto suite" << selected_crypto_suite;
    return false;
  }

  // OK, we're now doing DTLS (RFC 5764)
  std::vector<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);

  // RFC 5705 exporter using the RFC 5764 parameters
  if (!transport->ExportKeyingMaterial(kDtlsSrtpExporterLabel, NULL, 0, false,
                                       &dtls_buffer[0], dtls_buffer.size())) {
    LOG(LS_WARNING) << "DTLS-SRTP key export failed";
    RTC_NOTREACHED();  // This should never happen
    return false;
  }

  // Sync up the keys with the DTLS-SRTP interface
  std::vector<unsigned char> client_write_key(key_len + salt_len);
  std::vector<unsigned char> server_write_key(key_len + salt_len);
  size_t offset = 0;
  memcpy(&client_write_key[0], &dtls_buffer[offset], key_len);
  offset += key_len;
  memcpy(&server_write_key[0], &dtls_buffer[offset], key_len);
  offset += key_len;
  memcpy(&client_write_key[key_len], &dtls_buffer[offset], salt_len);
  offset += salt_len;
  memcpy(&server_write_key[key_len], &dtls_buffer[offset], salt_len);

  std::vector<unsigned char>*send_key, *recv_key;
  rtc::SSLRole role;
  if (!transport->GetSslRole(&role)) {
    LOG(LS_WARNING) << "GetSslRole failed";
    return false;
  }

  if (role == rtc::SSL_SERVER) {
    send_key = &server_write_key;
    recv_key = &client_write_key;
  } else {
    send_key = &client_write_key;
    recv_key = &server_write_key;
  }

  if (rtcp) {
    if (!IsActive()) {
      ret = srtp_transport_->SetRtcpParams(
          selected_crypto_suite, &(*send_key)[0],
          static_cast<int>(send_key->size()), selected_crypto_suite,
          &(*recv_key)[0], static_cast<int>(recv_key->size()));
    } else {
      // RTCP doesn't need to call SetRtpParam because it is only used
      // to make the updated encrypted RTP header extension IDs take effect.
      ret = true;
    }
  } else {
    ret = srtp_transport_->SetRtpParams(selected_crypto_suite, &(*send_key)[0],
                                        static_cast<int>(send_key->size()),
                                        selected_crypto_suite, &(*recv_key)[0],
                                        static_cast<int>(recv_key->size()));
  }

  if (!ret) {
    LOG(LS_WARNING) << "DTLS-SRTP key installation failed";
  }
  // The user of DtlsSrtpTransport needs to update SRTP overhead explicitly
  // which used to be part of this method.
  return ret;
}

void DtlsSrtpTransport::SetSendEncryptedHeaderExtensionIds(
    const std::vector<int>& send_extension_ids) {
  srtp_transport_->SetSendEncryptedHeaderExtensionIds(send_extension_ids);
}

void DtlsSrtpTransport::SetRecvEncryptedHeaderExtensionIds(
    const std::vector<int>& recv_extension_ids) {
  srtp_transport_->SetRecvEncryptedHeaderExtensionIds(recv_extension_ids);
}

void DtlsSrtpTransport::SetRtcpMuxEnabled(bool enable) {
  srtp_transport_->SetRtcpMuxEnabled(enable);
}

rtc::PacketTransportInternal* DtlsSrtpTransport::rtp_packet_transport() const {
  return srtp_transport_->rtp_packet_transport();
}

void DtlsSrtpTransport::SetRtpPacketTransport(
    rtc::PacketTransportInternal* rtp) {
  srtp_transport_->SetRtpPacketTransport(rtp);
}

PacketTransportInterface* DtlsSrtpTransport::GetRtpPacketTransport() const {
  return srtp_transport_->GetRtpPacketTransport();
}

rtc::PacketTransportInternal* DtlsSrtpTransport::rtcp_packet_transport() const {
  return srtp_transport_->rtcp_packet_transport();
}

void DtlsSrtpTransport::SetRtcpPacketTransport(
    rtc::PacketTransportInternal* rtcp) {
  srtp_transport_->SetRtcpPacketTransport(rtcp);
}

PacketTransportInterface* DtlsSrtpTransport::GetRtcpPacketTransport() const {
  return srtp_transport_->GetRtcpPacketTransport();
}

bool DtlsSrtpTransport::IsWritable(bool rtcp) const {
  return srtp_transport_->IsWritable(rtcp);
}

bool DtlsSrtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                      const rtc::PacketOptions& options,
                                      int flags) {
  return srtp_transport_->SendRtpPacket(packet, options, flags);
}

bool DtlsSrtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                       const rtc::PacketOptions& options,
                                       int flags) {
  return srtp_transport_->SendRtcpPacket(packet, options, flags);
}

bool DtlsSrtpTransport::HandlesPayloadType(int payload_type) const {
  return srtp_transport_->HandlesPayloadType(payload_type);
}

void DtlsSrtpTransport::AddHandledPayloadType(int payload_type) {
  srtp_transport_->AddHandledPayloadType(payload_type);
}

void DtlsSrtpTransport::ResetParams() {
  srtp_transport_->ResetParams();
}

RTCError DtlsSrtpTransport::SetParameters(
    const RtpTransportParameters& parameters) {
  return srtp_transport_->SetParameters(parameters);
}

RtpTransportParameters DtlsSrtpTransport::GetParameters() const {
  return srtp_transport_->GetParameters();
}

}  // namespace webrtc
