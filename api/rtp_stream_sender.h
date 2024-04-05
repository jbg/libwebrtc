/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpSenders
// http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface

#ifndef API_RTP_STREAM_SENDER_H_
#define API_RTP_STREAM_SENDER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RtpStreamSender : public webrtc::RefCountInterface {
  public:
  class RtpStreamSenderPacket {
  public:
    uint32_t Ssrc() {
      return ssrc_;
    }
    std::vector<uint32_t> Csrcs() {
      return csrcs_;
    }

    uint8_t PayloadType() {
      return payload_type_;
    }

    uint32_t RtpTimestamp {
      return rtp_timestamp_;
    }

  private:
    uint32_t ssrc_;
    std::vector<uint32_t> csrcs_;
    uint8_t payload_type_;
    uint32_t rtp_timestamp_;

    bool marker_;
    uint8_t padding_size_;
    uint16_t sequence_number_;
    size_t payload_offset_;  // Match header size with csrcs and extensions.
    size_t payload_size_;

    size_t extensions_size_ = 0;  // Unaligned.
    rtc::CopyOnWriteBuffer buffer_;

    webrtc::Timestamp capture_time_ = webrtc::Timestamp::Zero();
    absl::optional<RtpPacketMediaType> packet_type_;
    absl::optional<int64_t> transport_sequence_number_;
    bool allow_retransmission_ = false;
    absl::optional<uint16_t> retransmitted_sequence_number_;
    rtc::scoped_refptr<rtc::RefCountedBase> additional_data_;
    bool is_first_packet_of_frame_ = false;
    bool is_key_frame_ = false;
    bool fec_protect_packet_ = false;
    bool is_red_ = false;
    absl::optional<TimeDelta> time_in_send_queue_;
  };

 virtual void SendRtp(std::unique_ptr<RtpStreamSenderPacket> packet) = 0;

 protected:
  ~RtpStreamSender() override = default;
};

}  // namespace webrtc

#endif  // API_RTP_STREAM_SENDER_H_
