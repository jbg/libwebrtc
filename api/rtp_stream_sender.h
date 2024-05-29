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
    class HeaderExtension {
    public:
      std::string uri;
      rtc::ArrayView<const uint8_t> value;
    };


    // TODO: Make this not a horror. Builder pattern?
    RtpStreamSenderPacket(std::vector<uint32_t> csrcs,
        uint8_t payload_type, uint32_t rtp_timestamp, bool is_first_packet_of_frame,
        bool is_key_frame, bool marker, rtc::ArrayView<const uint8_t> data,
        std::vector<HeaderExtension> header_extensions) :
        csrcs_(csrcs), payload_type_(payload_type),
        rtp_timestamp_(rtp_timestamp), is_first_packet_of_frame_(is_first_packet_of_frame),
        is_key_frame_(is_key_frame), marker_(marker), data_(data),
        header_extensions_(header_extensions) {}

    const std::vector<uint32_t>& Csrcs() {
      return csrcs_;
    }

    uint8_t PayloadType() {
      return payload_type_;
    }

    uint32_t RtpTimestamp() {
      return rtp_timestamp_;
    }

    rtc::ArrayView<const uint8_t> Data() {
      return data_;
    }

    bool IsFirstPacketOfFrame() {
      return is_first_packet_of_frame_;
    }

    bool IsKeyFrame() {
      return is_key_frame_;
    }

    bool Marker() {
      return marker_;
    }

    const std::vector<HeaderExtension>& HeaderExtensions() {
      return header_extensions_;
    }

  private:
    std::vector<uint32_t> csrcs_;
    uint8_t payload_type_;
    uint32_t rtp_timestamp_;

    bool is_first_packet_of_frame_ = false;
    bool is_key_frame_ = false;
    bool marker_;

    rtc::ArrayView<const uint8_t> data_;
    std::vector<HeaderExtension> header_extensions_;
  };

 virtual void SendRtp(std::unique_ptr<RtpStreamSenderPacket> packet) = 0;

 class PacketHandler {
  public:
  virtual ~PacketHandler() = default;
  virtual void OnPackets(std::vector<std::unique_ptr<RtpStreamSenderPacket>> packets) = 0;
 };

 virtual void RegisterPacketHandler(std::unique_ptr<PacketHandler> packet_handler) = 0;

 protected:
  ~RtpStreamSender() override = default;
};

}  // namespace webrtc

#endif  // API_RTP_STREAM_SENDER_H_
