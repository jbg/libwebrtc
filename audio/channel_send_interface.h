/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AUDIO_CHANNEL_SEND_INTERFACE_H_
#define AUDIO_CHANNEL_SEND_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/call/transport.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/function_view.h"

namespace webrtc {

struct CallSendStatistics {
  int64_t rttMs;
  size_t bytesSent;
  int packetsSent;
};

// See section 6.4.2 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct ReportBlock {
  uint32_t sender_SSRC;  // SSRC of sender
  uint32_t source_SSRC;
  uint8_t fraction_lost;
  int32_t cumulative_num_packets_lost;
  uint32_t extended_highest_sequence_number;
  uint32_t interarrival_jitter;
  uint32_t last_SR_timestamp;
  uint32_t delay_since_last_SR;
};

namespace voe {

class ChannelSendInterface {
 public:
  virtual ~ChannelSendInterface() = default;

  // Shared with ChannelReceiveProxy
  virtual void SetLocalSSRC(uint32_t ssrc) = 0;
  virtual void SetNACKStatus(bool enable, int max_packets) = 0;
  virtual CallSendStatistics GetRTCPStatistics() const = 0;
  virtual void RegisterTransport(Transport* transport) = 0;
  virtual bool ReceivedRTCPPacket(const uint8_t* packet, size_t length) = 0;

  virtual bool SetEncoder(int payload_type,
                          std::unique_ptr<AudioEncoder> encoder) = 0;
  virtual void ModifyEncoder(
      rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) = 0;

  virtual void SetRTCPStatus(bool enable) = 0;
  virtual void SetMid(const std::string& mid, int extension_id) = 0;
  virtual void SetRTCP_CNAME(const std::string& c_name) = 0;
  virtual void SetSendAudioLevelIndicationStatus(bool enable, int id) = 0;
  virtual void EnableSendTransportSequenceNumber(int id) = 0;
  virtual void RegisterSenderCongestionControlObjects(
      RtpTransportControllerSendInterface* transport,
      RtcpBandwidthObserver* bandwidth_observer) = 0;
  virtual void ResetSenderCongestionControlObjects() = 0;
  virtual std::vector<ReportBlock> GetRemoteRTCPReportBlocks() const = 0;
  virtual ANAStats GetANAStatistics() const = 0;
  virtual bool SetSendTelephoneEventPayloadType(int payload_type,
                                                int payload_frequency) = 0;
  virtual bool SendTelephoneEventOutband(int event, int duration_ms) = 0;
  virtual void SetBitrate(int bitrate_bps, int64_t probing_interval_ms) = 0;
  virtual void SetInputMute(bool muted) = 0;

  virtual void ProcessAndEncodeAudio(
      std::unique_ptr<AudioFrame> audio_frame) = 0;
  virtual void SetTransportOverhead(int transport_overhead_per_packet) = 0;
  virtual RtpRtcp* GetRtpRtcp() const = 0;

  virtual void OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) = 0;
  virtual void OnRecoverableUplinkPacketLossRate(
      float recoverable_packet_loss_rate) = 0;
  virtual void StartSend() = 0;
  virtual void StopSend() = 0;

  // Needed by ChannelReceiveProxy::AssociateSendChannel.
  virtual int64_t GetRTT() const = 0;

  // E2EE Custom Audio Frame Encryption (Optional)
  virtual void SetFrameEncryptor(FrameEncryptorInterface* frame_encryptor) = 0;
};
}  // namespace voe
}  // namespace webrtc

#endif  // AUDIO_CHANNEL_SEND_INTERFACE_H_
