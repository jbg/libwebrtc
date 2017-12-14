/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_HEADERS_H_
#define API_RTP_HEADERS_H_

#include <stddef.h>
#include <string.h>
#include <ostream>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/optional.h"
#include "api/video/video_content_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"

#include "rtc_base/checks.h"
#include "rtc_base/deprecation.h"
#include "common_types.h"
#include "typedefs.h"

namespace webrtc {

// Class to represent the value of RTP header extensions that are
// variable-length strings (e.g., RtpStreamId and RtpMid).
// Unlike std::string, it can be copied with memcpy and cleared with memset.
//
// Empty value represents unset header extension (use empty() to query).
class StringRtpHeaderExtension {
 public:
  // String RTP header extensions are limited to 16 bytes because it is the
  // maximum length that can be encoded with one-byte header extensions.
  static constexpr size_t kMaxSize = 16;

  static bool IsLegalName(rtc::ArrayView<const char> name);

  StringRtpHeaderExtension() { value_[0] = 0; }
  explicit StringRtpHeaderExtension(rtc::ArrayView<const char> value) {
    Set(value.data(), value.size());
  }
  StringRtpHeaderExtension(const StringRtpHeaderExtension&) = default;
  StringRtpHeaderExtension& operator=(const StringRtpHeaderExtension&) =
      default;

  bool empty() const { return value_[0] == 0; }
  const char* data() const { return value_; }
  size_t size() const { return strnlen(value_, kMaxSize); }

  void Set(rtc::ArrayView<const uint8_t> value) {
    Set(reinterpret_cast<const char*>(value.data()), value.size());
  }
  void Set(const char* data, size_t size);

  friend bool operator==(const StringRtpHeaderExtension& lhs,
                         const StringRtpHeaderExtension& rhs) {
    return strncmp(lhs.value_, rhs.value_, kMaxSize) == 0;
  }
  friend bool operator!=(const StringRtpHeaderExtension& lhs,
                         const StringRtpHeaderExtension& rhs) {
    return !(lhs == rhs);
  }

 private:
  char value_[kMaxSize];
};

// StreamId represents RtpStreamId which is a string.
typedef StringRtpHeaderExtension StreamId;

// Mid represents RtpMid which is a string.
typedef StringRtpHeaderExtension Mid;

struct RTPHeaderExtension {
  RTPHeaderExtension();
  RTPHeaderExtension(const RTPHeaderExtension& other);
  RTPHeaderExtension& operator=(const RTPHeaderExtension& other);

  bool hasTransmissionTimeOffset;
  int32_t transmissionTimeOffset;
  bool hasAbsoluteSendTime;
  uint32_t absoluteSendTime;
  bool hasTransportSequenceNumber;
  uint16_t transportSequenceNumber;

  // Audio Level includes both level in dBov and voiced/unvoiced bit. See:
  // https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
  bool hasAudioLevel;
  bool voiceActivity;
  uint8_t audioLevel;

  // For Coordination of Video Orientation. See
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
  // ts_126114v120700p.pdf
  bool hasVideoRotation;
  VideoRotation videoRotation;

  // TODO(ilnik): Refactor this and one above to be rtc::Optional() and remove
  // a corresponding bool flag.
  bool hasVideoContentType;
  VideoContentType videoContentType;

  bool has_video_timing;
  VideoSendTiming video_timing;

  PlayoutDelay playout_delay = {-1, -1};

  // For identification of a stream when ssrc is not signaled. See
  // https://tools.ietf.org/html/draft-ietf-avtext-rid-09
  // TODO(danilchap): Update url from draft to release version.
  StreamId stream_id;
  StreamId repaired_stream_id;

  // For identifying the media section used to interpret this RTP packet. See
  // https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-38
  Mid mid;
};

struct RTPHeader {
  RTPHeader();
  RTPHeader(const RTPHeader& other);
  RTPHeader& operator=(const RTPHeader& other);

  bool markerBit;
  uint8_t payloadType;
  uint16_t sequenceNumber;
  uint32_t timestamp;
  uint32_t ssrc;
  uint8_t numCSRCs;
  uint32_t arrOfCSRCs[kRtpCsrcSize];
  size_t paddingLength;
  size_t headerLength;
  int payload_type_frequency;
  RTPHeaderExtension extension;
};

struct RtpPacketCounter {
  RtpPacketCounter()
      : header_bytes(0), payload_bytes(0), padding_bytes(0), packets(0) {}

  void Add(const RtpPacketCounter& other) {
    header_bytes += other.header_bytes;
    payload_bytes += other.payload_bytes;
    padding_bytes += other.padding_bytes;
    packets += other.packets;
  }

  void Subtract(const RtpPacketCounter& other) {
    RTC_DCHECK_GE(header_bytes, other.header_bytes);
    header_bytes -= other.header_bytes;
    RTC_DCHECK_GE(payload_bytes, other.payload_bytes);
    payload_bytes -= other.payload_bytes;
    RTC_DCHECK_GE(padding_bytes, other.padding_bytes);
    padding_bytes -= other.padding_bytes;
    RTC_DCHECK_GE(packets, other.packets);
    packets -= other.packets;
  }

  void AddPacket(size_t packet_length, const RTPHeader& header) {
    ++packets;
    header_bytes += header.headerLength;
    padding_bytes += header.paddingLength;
    payload_bytes +=
        packet_length - (header.headerLength + header.paddingLength);
  }

  size_t TotalBytes() const {
    return header_bytes + payload_bytes + padding_bytes;
  }

  size_t header_bytes;   // Number of bytes used by RTP headers.
  size_t payload_bytes;  // Payload bytes, excluding RTP headers and padding.
  size_t padding_bytes;  // Number of padding bytes.
  uint32_t packets;      // Number of packets.
};

// Data usage statistics for a (rtp) stream.
struct StreamDataCounters {
  StreamDataCounters();

  void Add(const StreamDataCounters& other) {
    transmitted.Add(other.transmitted);
    retransmitted.Add(other.retransmitted);
    fec.Add(other.fec);
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms < first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use oldest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  void Subtract(const StreamDataCounters& other) {
    transmitted.Subtract(other.transmitted);
    retransmitted.Subtract(other.retransmitted);
    fec.Subtract(other.fec);
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms > first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use youngest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  int64_t TimeSinceFirstPacketInMs(int64_t now_ms) const {
    return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
  }

  // Returns the number of bytes corresponding to the actual media payload (i.e.
  // RTP headers, padding, retransmissions and fec packets are excluded).
  // Note this function does not have meaning for an RTX stream.
  size_t MediaPayloadBytes() const {
    return transmitted.payload_bytes - retransmitted.payload_bytes -
           fec.payload_bytes;
  }

  int64_t first_packet_time_ms;    // Time when first packet is sent/received.
  RtpPacketCounter transmitted;    // Number of transmitted packets/bytes.
  RtpPacketCounter retransmitted;  // Number of retransmitted packets/bytes.
  RtpPacketCounter fec;            // Number of redundancy packets/bytes.
};

// Callback, called whenever byte/packet counts have been updated.
class StreamDataCountersCallback {
 public:
  virtual ~StreamDataCountersCallback() {}

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) = 0;
};

// RTCP mode to use. Compound mode is described by RFC 4585 and reduced-size
// RTCP mode is described by RFC 5506.
enum class RtcpMode { kOff, kCompound, kReducedSize };

enum NetworkState {
  kNetworkUp,
  kNetworkDown,
};

struct RtpKeepAliveConfig final {
  // If no packet has been sent for |timeout_interval_ms|, send a keep-alive
  // packet. The keep-alive packet is an empty (no payload) RTP packet with a
  // payload type of 20 as long as the other end has not negotiated the use of
  // this value. If this value has already been negotiated, then some other
  // unused static payload type from table 5 of RFC 3551 shall be used and set
  // in |payload_type|.
  int64_t timeout_interval_ms = -1;
  uint8_t payload_type = 20;

  bool operator==(const RtpKeepAliveConfig& o) const {
    return timeout_interval_ms == o.timeout_interval_ms &&
           payload_type == o.payload_type;
  }
  bool operator!=(const RtpKeepAliveConfig& o) const { return !(*this == o); }
};

// Currently only VP8/VP9 specific.
struct RtpPayloadState {
  int16_t picture_id = -1;
};

}  // namespace webrtc

#endif  // API_RTP_HEADERS_H_
