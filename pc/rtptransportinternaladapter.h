/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPTRANSPORTINTERNALADAPTER_H_
#define PC_RTPTRANSPORTINTERNALADAPTER_H_

#include <memory>
#include <utility>

#include "pc/rtptransportinternal.h"

namespace webrtc {

class RtpTransportInternalAdapter : public RtpTransportInternal {
 public:
  RtpTransportInternalAdapter(
      std::unique_ptr<RtpTransportInternal> rtp_transport)
      : owned_transport_(std::move(rtp_transport)) {
    RTC_DCHECK(owned_transport_);
  }

  void SetRtcpMuxEnabled(bool enable) override {
    owned_transport_->SetRtcpMuxEnabled(enable);
  }

  rtc::PacketTransportInternal* rtp_packet_transport() const override {
    return owned_transport_->rtp_packet_transport();
  }
  void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp) override {
    owned_transport_->SetRtpPacketTransport(rtp);
  }

  rtc::PacketTransportInternal* rtcp_packet_transport() const override {
    return owned_transport_->rtcp_packet_transport();
  }
  void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp) override {
    owned_transport_->SetRtcpPacketTransport(rtcp);
  }

  bool IsWritable(bool rtcp) const override {
    return owned_transport_->IsWritable(rtcp);
  }

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override {
    return owned_transport_->SendRtpPacket(packet, options, flags);
  }

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override {
    return owned_transport_->SendRtcpPacket(packet, options, flags);
  }

  bool HandlesPayloadType(int payload_type) const override {
    return owned_transport_->HandlesPayloadType(payload_type);
  }

  void AddHandledPayloadType(int payload_type) override {
    return owned_transport_->AddHandledPayloadType(payload_type);
  }

  // RtpTransportInterface overrides.
  PacketTransportInterface* GetRtpPacketTransport() const override {
    return owned_transport_->GetRtpPacketTransport();
  }

  PacketTransportInterface* GetRtcpPacketTransport() const override {
    return owned_transport_->GetRtcpPacketTransport();
  }

  RTCError SetParameters(const RtpTransportParameters& parameters) override {
    return owned_transport_->SetParameters(parameters);
  }

  RtpTransportParameters GetParameters() const override {
    return owned_transport_->GetParameters();
  }

 protected:
  std::unique_ptr<RtpTransportInternal> owned_transport_;
};

}  // namespace webrtc

#endif  // PC_RTPTRANSPORTINTERNALADAPTER_H_
