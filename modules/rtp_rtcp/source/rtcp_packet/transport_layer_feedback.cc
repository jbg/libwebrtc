/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/transport_layer_feedback.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/network/ecn_marking.h"

namespace webrtc {
namespace rtcp {

/*
0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |V=2|P| FMT=11  |   PT = 205    |          length               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 SSRC of RTCP packet sender                    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                   SSRC of 1st RTP Stream                      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |          begin_seq            |          num_reports          |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |R|ECN|  Arrival time offset    | ...                           .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     .                                                               .
     .                                                               .
     .                                                               .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                   SSRC of nth RTP Stream                      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |          begin_seq            |          num_reports          |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |R|ECN|  Arrival time offset    | ...                           |
     .                                                               .
     .                                                               .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 Report Timestamp (32 bits)                    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

namespace {

constexpr size_t kSenderSsrcLength = 4;
constexpr size_t kHeaderPerMediaSssrcLength = 8;
constexpr size_t kPerPacketLength = 2;
constexpr size_t kTimestampLength = 4;

// RFC-3168, Section 5 but shifted up.
constexpr uint16_t kEcnEct1 = 0x01;
constexpr uint16_t kEcnEct0 = 0x02;
constexpr uint16_t kEcnCe = 0x03;

// Arrival time offset (ATO, 13 bits):
// The arrival time of the RTP packet at the receiver, as an offset before the
// time represented by the Report Timestamp (RTS) field of this RTCP congestion
// control feedback report. The ATO field is in units of 1/1024 seconds (this
// unit is chosen to give exact offsets from the RTS field) so, for example, an
// ATO value of 512 indicates that the corresponding RTP packet arrived exactly
// half a second before the time instant represented by the RTS field. If the
// measured value is greater than 8189/1024 seconds (the value that would be
// coded as 0x1FFD), the value 0x1FFE MUST be reported to indicate an over-range
// measurement. If the measurement is unavailable or if the arrival time of the
// RTP packet is after the time represented by the RTS field, then an ATO value
// of 0x1FFF MUST be reported for the packet.
uint16_t to_13bit_ato(TimeDelta arrival_time_offset) {
  if (arrival_time_offset.ms() > 8189) {
    return 0x1FFE;
  }
  if (arrival_time_offset < TimeDelta::Zero()) {
    return 0x1FFF;
  }
  return static_cast<uint16_t>(1024 * arrival_time_offset.seconds<float>());
}

// Packet info in below methods refere to 16 per packet formated as:
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |R|ECN|  Arrival time offset    | ...                           .
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TimeDelta ato_to_timedelta(uint16_t packet_info) {
  const uint16_t ato = packet_info & 0x1FFF;
  if (ato >= 0x1FFE) {
    return TimeDelta::PlusInfinity();
  }
  if (ato > 0x1FFF) {
    return TimeDelta::MinusInfinity();
  }
  return TimeDelta::Seconds(ato / 1024.0f);
}

uint16_t to_2_bit_ecn(rtc::EcnMarking ecn_marking) {
  switch (ecn_marking) {
    case rtc::EcnMarking::kNotEct:
      return 0;
    case rtc::EcnMarking::kEct1:
      return kEcnEct1 << 14;
    case rtc::EcnMarking::kEct0:
      return kEcnCe << 14;
    case rtc::EcnMarking::kCe:
      return kEcnCe << 14;
  }
}

rtc::EcnMarking to_ecn_marking(uint16_t packet_info) {
  const uint16_t ecn = (packet_info & 0x6000) >> 14;
  if (ecn == kEcnEct1) {
    return rtc::EcnMarking::kEct1;
  }
  if (ecn == kEcnEct0) {
    return rtc::EcnMarking::kEct0;
  }
  if (ecn == kEcnCe) {
    return rtc::EcnMarking::kCe;
  }
  return rtc::EcnMarking::kNotEct;
}

}  // namespace

TransportLayerFeedback ::TransportLayerFeedback(
    std::map<uint32_t /*ssrc*/, std::vector<PacketInfo>> packets,
    uint32_t compact_ntp_timestamp)
    : packets_(std::move(packets)),
      compact_ntp_timestamp_(compact_ntp_timestamp) {}

bool TransportLayerFeedback::Create(uint8_t* packet,
                                    size_t* position,
                                    size_t max_length,
                                    PacketReadyCallback callback) const {
  // Ensure there is enough room for this packet.
  while (*position + BlockLength() > max_length) {
    if (!OnBufferFull(packet, position, callback))
      return false;
  }

  const size_t position_end = *position + BlockLength();

  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               position);

  ByteWriter<uint32_t>::WriteBigEndian(&packet[*position], sender_ssrc());
  *position += 4;

  // From the RFC:
  // "RTCP Congestion Control Feedback Packets SHOULD include a report
  // block for every active SSRC."
  // "The value of num_reports MAY be 0, indicating that there are no packet
  // metric blocks included for that SSRC.""
  // If 50 streams are received, each with 2 SSRC, feedback can be quite
  // large. So for now, we ignore and only send feedback of received packets.
  // Since a sender knows when a packet is sent, it can figure out if all
  // packets from a SSRC has been lost if at least one packet is received.
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                   SSRC of nth RTP Stream                      |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |          begin_seq            |          num_reports          |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |R|ECN|  Arrival time offset    | ...                           .
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   .                                                               .
  //   .                                                               .
  for (const auto& [ssrc, packets] : packets_) {
    if (packets.empty()) {
      continue;
    }

    ByteWriter<uint32_t>::WriteBigEndian(&packet[*position], ssrc);
    *position += 4;

    ByteWriter<uint16_t>::WriteBigEndian(&packet[*position],
                                         packets.begin()->sequence_number);
    *position += 2;
    uint16_t num_reports =
        packets.back().sequence_number - packets.front().sequence_number + 1;

    // Each report block MUST NOT include more than 16384 packet metric blocks
    // (i.e., it MUST NOT report on more than one quarter of the sequence number
    // space in a single report).
    if (num_reports > 16384) {
      RTC_DCHECK(num_reports < 16384);
      return false;
    }
    ByteWriter<uint16_t>::WriteBigEndian(&packet[*position], num_reports);
    *position += 2;

    int packet_index = 0;
    for (int i = 0; i < num_reports; ++i) {
      uint16_t sequence_number = packets.front().sequence_number + i;

      bool received = sequence_number == packets[packet_index].sequence_number;

      uint16_t ato =
          received ? to_13bit_ato(packets[packet_index].arrival_time_offset)
                   : 0;
      uint16_t ecn = received ? to_2_bit_ecn(packets[packet_index].ecn) : 0;
      uint16_t packet_info = (received ? 0x8000 : 0) | ecn | ato;
      if (received) {
        ++packet_index;
      }
      ByteWriter<uint16_t>::WriteBigEndian(&packet[*position], packet_info);
      *position += 2;
    }
    // 32bit allign per SSRC block.
    if (num_reports % 2 != 0) {
      ByteWriter<uint16_t>::WriteBigEndian(&packet[*position], 0);
      *position += 2;
    }
  }

  ByteWriter<uint32_t>::WriteBigEndian(&packet[*position],
                                       compact_ntp_timestamp_);
  *position += 4;
  RTC_DCHECK_EQ(*position, position_end);
  return true;
}

size_t TransportLayerFeedback::BlockLength() const {
  // Total size of this packet

  size_t total_size = kSenderSsrcLength + kHeaderLength + kTimestampLength;
  for (const auto& [ssrc, packets] : packets_) {
    if (packets.empty()) {
      continue;
    }
    total_size += kHeaderPerMediaSssrcLength;
    uint16_t num_reports =
        packets.back().sequence_number - packets.front().sequence_number + 1;
    size_t packet_block_size = num_reports * kPerPacketLength;
    total_size += packet_block_size + packet_block_size % 4;
  }
  return total_size;
}

bool TransportLayerFeedback::Parse(const rtcp::CommonHeader& packet) {
  const uint8_t* payload = packet.payload();

  size_t position = 0;
  size_t max_position = packet.payload_size_bytes();

  if (packet.payload_size_bytes() % 4 != 0 ||
      packet.payload_size_bytes() <
          kHeaderPerMediaSssrcLength + kTimestampLength) {
    return -1;
  }

  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[position]));
  position += 4;

  compact_ntp_timestamp_ =
      ByteReader<uint32_t>::ReadBigEndian(&payload[max_position - 4]);

  while (position <
         max_position - kHeaderPerMediaSssrcLength - kTimestampLength) {
    uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&payload[position]);
    position += 4;

    uint16_t base_seqno =
        ByteReader<uint16_t>::ReadBigEndian(&payload[position]);
    position += 2;
    uint16_t num_reports =
        ByteReader<uint16_t>::ReadBigEndian(&payload[position]);
    position += 2;

    for (int i = 0;
         i < num_reports && position < max_position - kPerPacketLength; ++i) {
      uint16_t packet_info =
          ByteReader<uint16_t>::ReadBigEndian(&payload[position]);
      position += 2;

      uint16_t seq_no = base_seqno + i;
      bool received = (packet_info & 0x8000);
      if (received) {
        packets_[ssrc].push_back(
            {.sequence_number = seq_no,
             .arrival_time_offset = ato_to_timedelta(packet_info),
             .ecn = to_ecn_marking((packet_info & 0x6000) >> 14)});
      }
    }
    if (num_reports % 2) {
      // 2 bytes padding
      position += 2;
    }
  }
  return position + kTimestampLength == max_position;
}
}  // namespace rtcp
}  // namespace webrtc
