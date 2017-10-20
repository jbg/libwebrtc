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
#include "pc/rtptransportinternaladapter.h"
#include "pc/srtptransport.h"

namespace webrtc {

// This class exports the keying materials from the DtlsTransport underneath and
// sets the crypto keys for the wrapped SrtpTransport.
class DtlsSrtpTransport : public RtpTransportInternalAdapter {
 public:
  explicit DtlsSrtpTransport(
      std::unique_ptr<webrtc::SrtpTransport> srtp_transport);

  // Set P2P layer RTP/RTCP DtlsTransports.
  void SetDtlsTransports(cricket::DtlsTransportInternal* rtp_dtls_transport,
                         cricket::DtlsTransportInternal* rtcp_dtls_transport);

  void SetRtcpMuxEnabled(bool enable) override;

  // Set the header extension ids that should be encrypted.
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

  bool IsActive() { return srtp_transport_->IsActive(); }

  // TODO(zhihuang): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override { return nullptr; }

  sigslot::signal2<DtlsSrtpTransport*, bool> SignalDtlsSrtpSetupFailure;

 private:
  bool CanSetupDtlsSrtp();
  void MaybeSetupDtlsSrtp();
  bool SetupDtlsSrtp(bool rtcp);
  void ConnectToSrtpTransport();
  void SetDtlsTransport(bool rtcp,
                        cricket::DtlsTransportInternal* dtls_transport);
  void UpdateWritableState();

  void OnDtlsState(cricket::DtlsTransportInternal* dtls_transport,
                   cricket::DtlsTransportState state);
  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time);
  void OnReadyToSend(bool ready);

  bool writable_ = false;
  std::unique_ptr<SrtpTransport> srtp_transport_;
  // Owned by the TransportController.
  cricket::DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  cricket::DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;
};

}  // namespace webrtc

#endif  // PC_DTLSSRTPTRANSPORT_H_
