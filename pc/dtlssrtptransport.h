/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DTLSSRTPTRANSPORT_H_
#define PC_DTLSSRTPTRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "p2p/base/dtlstransportinternal.h"
#include "pc/srtptransport.h"

namespace webrtc {

// This class exports the keying materials from the DtlsTransport underneath and
// sets the crypto keys for the wrapped SrtpTransport.
class DtlsSrtpTransport : public RtpTransportInternal {
 public:
  explicit DtlsSrtpTransport(
      std::unique_ptr<webrtc::SrtpTransport> srtp_transport);

  bool SetupDtlsSrtp(bool rtcp);

  // Set a P2P layer DtlsTransport for RTP DtlsSrtpTransport.
  void SetRtpDtlsTransport(cricket::DtlsTransportInternal* dtls_transport) {
    rtp_dtls_transport_ = dtls_transport;
  }

  // Set a P2P layer DtlsTransport for RTCP DtlsSrtpTransport.
  void SetRtcpDtlsTransport(cricket::DtlsTransportInternal* dtls_transport) {
    rtcp_dtls_transport_ = dtls_transport;
  }
  // Set the header extension ids that should be encrypted.
  // This method doesn't immediately update the SRTP session with the new IDs,
  // and you need to call SetupDtlsSrtp for that to happen.
  void SetSendEncryptedHeaderExtensionIds(
      const std::vector<int>& send_extension_ids);

  void SetRecvEncryptedHeaderExtensionIds(
      const std::vector<int>& recv_extension_ids);

  cricket::DtlsTransportInternal* rtp_dtls_transport() {
    return rtp_dtls_transport_;
  }

  cricket::DtlsTransportInternal* rtcp_dtls_transport() {
    return rtcp_dtls_transport_;
  }

  // RtpTransportInternal overrides.
  void SetRtcpMuxEnabled(bool enable) override;
  rtc::PacketTransportInternal* rtp_packet_transport() const override;
  void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp) override;
  PacketTransportInterface* GetRtpPacketTransport() const override;
  rtc::PacketTransportInternal* rtcp_packet_transport() const override;
  void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp) override;
  PacketTransportInterface* GetRtcpPacketTransport() const override;
  bool IsWritable(bool rtcp) const override;
  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;
  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;
  bool HandlesPayloadType(int payload_type) const override;
  void AddHandledPayloadType(int payload_type) override;

  bool IsActive() { return srtp_transport_->IsActive(); }

  void ResetParams();

  RTCError SetParameters(const RtpTransportParameters& parameters) override;

  RtpTransportParameters GetParameters() const override;

  // TODO(zhihuang): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override { return nullptr; }

 private:
  void ConnectToSrtpTransport();

  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time);

  void OnReadyToSend(bool ready);

  std::unique_ptr<SrtpTransport> srtp_transport_;
  // Owned by the TransportController.
  cricket::DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  cricket::DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;
};

}  // namespace webrtc

#endif  // PC_DTLSSRTPTRANSPORT_H_
