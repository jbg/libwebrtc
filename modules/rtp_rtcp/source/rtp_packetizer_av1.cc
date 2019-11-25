/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_packetizer_av1.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// TODO(danilchap): Some of the helpers/constants are same as in
// rtp_depacketizer_av1. Move them to common av1 file.
constexpr int kAggregationHeaderSize = 1;
// when there are 3 or less OBU (fragments) in a packet, size of the last one
// can be omited.
constexpr int kMaxNumObusToOmitSize = 3;
constexpr uint8_t kObuSizePresentBit = 0b0'0000'010;
constexpr int kObuTypeTemporalDelimiter = 2;
constexpr int kObuTypeTileList = 8;
constexpr int kObuTypePadding = 15;

bool ObuHasExtension(uint8_t obu_header) {
  return obu_header & 0b0'0000'100;
}

bool ObuHasSize(uint8_t obu_header) {
  return obu_header & kObuSizePresentBit;
}

int ObuType(uint8_t obu_header) {
  return (obu_header & 0b0'1111'000) >> 3;
}

int Leb128Size(int value) {
  RTC_DCHECK_GE(value, 0);
  int size = 0;
  while (value >= 0x80) {
    ++size;
    value >>= 7;
  }
  return size + 1;
}

// Returns number of bytes consumed.
int WriteLeb128(uint32_t value, uint8_t* buffer) {
  int size = 0;
  while (value >= 0x80) {
    buffer[size] = 0x80 | (value & 0x7F);
    ++size;
    value >>= 7;
  }
  buffer[size] = value;
  ++size;
  return size;
}

// Given |remaining_bytes| free bytes left in a packet, returns max size of an
// OBU fragment that can fit into the packet.
// i.e. MaxFragmentSize + Leb128Size(MaxFragmentSize) <= remaining_bytes.
int MaxFragmentSize(int remaining_bytes) {
  if (remaining_bytes <= 1) {
    return 0;
  }
  for (int i = 1;; ++i) {
    if (remaining_bytes < (1 << 7 * i) + i) {
      return remaining_bytes - i;
    }
  }
}

}  // namespace

RtpPacketizerAv1::RtpPacketizerAv1(rtc::ArrayView<const uint8_t> payload,
                                   RtpPacketizer::PayloadSizeLimits limits)
    : obus_(ParseObus(payload)) {
  if (obus_.empty()) {
    return;
  }
  // Ignore certian edge cases where packets should be very small. They are
  // inpractical but adds complexity to handle.
  if (limits.max_payload_len - limits.last_packet_reduction_len < 3 ||
      limits.max_payload_len - limits.first_packet_reduction_len < 3) {
    RTC_DLOG(LS_ERROR) << "Failed to packetize AV1 frame: requested packet "
                          "size is unreasonable small.";
    return;
  }
  // Aggregation header is present in all packets.
  limits.max_payload_len -= kAggregationHeaderSize;

  packets_ = Packetize(obus_, limits);
}

std::vector<RtpPacketizerAv1::Obu> RtpPacketizerAv1::ParseObus(
    rtc::ArrayView<const uint8_t> payload) {
  std::vector<Obu> result;
  rtc::ByteBufferReader remaining(reinterpret_cast<const char*>(payload.data()),
                                  payload.size());
  while (remaining.Length() > 0) {
    Obu obu;
    remaining.ReadUInt8(&obu.header);
    obu.size = 1;
    if (ObuHasExtension(obu.header)) {
      if (remaining.Length() == 0) {
        RTC_DLOG(LS_ERROR) << "Malformed AV1 input: expected extension_header, "
                              "no more bytes in the buffer. Offset: "
                           << (payload.size() - remaining.Length());
        return {};
      }
      remaining.ReadUInt8(&obu.extension_header);
      ++obu.size;
    }
    if (!ObuHasSize(obu.header)) {
      obu.payload =
          rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(remaining.Data()),
                             remaining.Length());
      remaining.Consume(remaining.Length());
    } else {
      uint64_t size = 0;
      if (!remaining.ReadUVarint(&size) || size > remaining.Length()) {
        RTC_DLOG(LS_ERROR) << "Malformed AV1 input: declared size " << size
                           << " is larger than remaining buffer size "
                           << remaining.Length();
        return {};
      }
      obu.payload = rtc::MakeArrayView(
          reinterpret_cast<const uint8_t*>(remaining.Data()), size);
      remaining.Consume(size);
    }
    obu.size += obu.payload.size();
    // Skip obus that shouldn't be transfered over rtp.
    int obu_type = ObuType(obu.header);
    if (obu_type != kObuTypeTemporalDelimiter &&  //
        obu_type != kObuTypeTileList &&           //
        obu_type != kObuTypePadding) {
      result.push_back(obu);
    }
  }
  return result;
}

int RtpPacketizerAv1::ExtraSizeForLastObu(const Packet& packet) {
  if (packet.packet_size == 0) {
    // Packet is still empty.
    return 0;
  }
  if (NumObus(packet) > kMaxNumObusToOmitSize) {
    // There is so many obus in the packet, space for the size of the last obu
    // element should be already reserved.
    return 0;
  }
  return Leb128Size(packet.last_obu_size);
}

std::vector<RtpPacketizerAv1::Packet> RtpPacketizerAv1::Packetize(
    rtc::ArrayView<const Obu> obus,
    PayloadSizeLimits limits) {
  std::vector<Packet> packets;
  // Assemble packets. Push to current packet as much as it can hold before
  // considering next one. That would normally cause uneven distribution across
  // packets, specifically last one would be generally smaller.
  packets.emplace_back(/*first_obu_index=*/0);
  int packet_remaining_bytes =
      limits.max_payload_len - limits.first_packet_reduction_len;
  for (size_t obu_index = 0; obu_index < obus.size(); ++obu_index) {
    const bool is_last_obu = obu_index == obus.size() - 1;
    const Obu& obu = obus[obu_index];

    int previous_obu_extra_size = ExtraSizeForLastObu(packets.back());
    int min_required_size =
        NumObus(packets.back()) >= kMaxNumObusToOmitSize ? 2 : 1;
    if (packet_remaining_bytes < previous_obu_extra_size + min_required_size) {
      // Start a new packet.
      packets.emplace_back(/*first_obu_index=*/obu_index);
      packet_remaining_bytes = limits.max_payload_len;
      previous_obu_extra_size = 0;
    }
    Packet& packet = packets.back();
    // Start inserting current obu into the packet.
    packet.packet_size += previous_obu_extra_size;
    packet_remaining_bytes -= previous_obu_extra_size;
    packet.last_obu = obu_index;

    bool must_write_obu_element_size = NumObus(packet) > kMaxNumObusToOmitSize;
    // Can fit all of the obu into the packet?
    int required_bytes = obu.size;
    if (must_write_obu_element_size) {
      required_bytes += Leb128Size(obu.size);
    }
    int available_bytes = packet_remaining_bytes;
    if (is_last_obu) {
      // If this packet would be the last packet, available size is smaller.
      if (packets.size() == 1) {
        available_bytes += limits.first_packet_reduction_len;
        available_bytes -= limits.single_packet_reduction_len;
      } else {
        available_bytes -= limits.last_packet_reduction_len;
      }
    }
    if (required_bytes <= available_bytes) {
      // Insert the obu into the packet unfragmented.
      packet.last_obu_size = obu.size;
      packet.packet_size += required_bytes;
      packet_remaining_bytes -= required_bytes;
      continue;
    }

    // Fragment the obu.
    int size_of_the_first_fragment =
        must_write_obu_element_size ? MaxFragmentSize(packet_remaining_bytes)
                                    : packet_remaining_bytes;
    // note that because available_bytes might be different than
    // packet_remaining_bytes it might happen that size_of_the_first_fragment >=
    // obu.size. Also, since checks above verified |obu| should not be put
    // completely into the |packet|, leave at least 1 byte for later packet.
    size_of_the_first_fragment =
        std::min(obu.size - 1, size_of_the_first_fragment);
    if (size_of_the_first_fragment == 0) {
      // Rather than writing 0-size element at the tail of the packet,
      // 'uninsert' the |obu| from the |packet|.
      packet.last_obu--;
      packet.packet_size -= previous_obu_extra_size;
    } else {
      packet.packet_size += size_of_the_first_fragment;
      if (must_write_obu_element_size) {
        packet.packet_size += Leb128Size(size_of_the_first_fragment);
      }
      packet.last_obu_size = size_of_the_first_fragment;
    }

    // Add middle fragments that occupy all of the packet.
    // These are easy because
    // - one obu per packet imply no need to store the size of the obu.
    // - this packets are nor the first nor the last packets of the frame, so
    // packet capacity is always limits.max_payload_len.
    int obu_offset;
    for (obu_offset = size_of_the_first_fragment;
         obu_offset + limits.max_payload_len < obu.size;
         obu_offset += limits.max_payload_len) {
      packets.emplace_back(/*first_obu_index=*/obu_index);
      Packet& packet = packets.back();
      packet.first_obu_offset = obu_offset;
      int size_of_the_middle_fragment = limits.max_payload_len;
      packet.last_obu_size = size_of_the_middle_fragment;
      packet.packet_size = size_of_the_middle_fragment;
    }

    // Add the last fragment of the obu.
    int size_of_the_last_fragment = obu.size - obu_offset;
    // Check for corner case where last fragment of the last obu is too large
    // to fit into last packet, but may fully fit into semi-last packet.
    if (is_last_obu &&
        size_of_the_last_fragment >
            limits.max_payload_len - limits.last_packet_reduction_len) {
      // Split last fragments into two.
      RTC_DCHECK_GE(size_of_the_last_fragment, 2);
      // Try to even packet sizes rather than payload sizes across the last
      // two packets.
      int size_of_the_semi_last_fragment =
          (size_of_the_last_fragment + limits.last_packet_reduction_len) / 2;
      // But leave at least one payload byte for the last packet to avoid
      // weird scenarios where size of the fragment is zero and rtp payload has
      // nothing except for an aggregation header.
      if (size_of_the_semi_last_fragment >= size_of_the_last_fragment) {
        size_of_the_semi_last_fragment = size_of_the_last_fragment - 1;
      }
      size_of_the_last_fragment -= size_of_the_semi_last_fragment;

      packets.emplace_back(/*first_obu_index=*/obu_index);
      Packet& packet = packets.back();
      packet.first_obu_offset = obu_offset;
      packet.last_obu_size = size_of_the_semi_last_fragment;
      packet.packet_size = size_of_the_semi_last_fragment;
      obu_offset += size_of_the_semi_last_fragment;
    }
    packets.emplace_back(/*first_obu_index=*/obu_index);
    Packet& last_packet = packets.back();
    last_packet.first_obu_offset = obu_offset;
    last_packet.last_obu_size = size_of_the_last_fragment;
    last_packet.packet_size = size_of_the_last_fragment;
    packet_remaining_bytes = limits.max_payload_len - size_of_the_last_fragment;
  }
  return packets;
}

uint8_t RtpPacketizerAv1::AggregationHeader(const Packet& packet) const {
  uint8_t aggregation_header = 0;

  // Set Z flag: first obu element is continuation of the previous OBU.
  bool first_obu_element_is_fragment = packet.first_obu_offset > 0;
  if (first_obu_element_is_fragment)
    aggregation_header |= (1 << 7);

  // Set Y flag: last obu element will be continuated in the next packet.
  int num_obus = NumObus(packet);
  int last_obu_offset = num_obus == 1 ? packet.first_obu_offset : 0;
  bool last_obu_is_fragment =
      last_obu_offset + packet.last_obu_size < obus_[packet.last_obu].size;
  if (last_obu_is_fragment)
    aggregation_header |= (1 << 6);

  // Set W field: number of obu elements in the packet (when not too large).
  if (num_obus <= kMaxNumObusToOmitSize)
    aggregation_header |= num_obus << 4;
  return aggregation_header;
}

bool RtpPacketizerAv1::NextPacket(RtpPacketToSend* packet) {
  if (packet_index_ >= packets_.size()) {
    return false;
  }
  Packet& next_packet = packets_[packet_index_];

  RTC_DCHECK_LT(next_packet.first_obu_offset,
                obus_[next_packet.first_obu].size);
  RTC_DCHECK_LE(next_packet.last_obu_size, obus_[next_packet.last_obu].size);

  uint8_t* const rtp_payload =
      packet->AllocatePayload(kAggregationHeaderSize + next_packet.packet_size);
  uint8_t* write_at = rtp_payload;

  *write_at++ = AggregationHeader(next_packet);

  int obu_offset = next_packet.first_obu_offset;
  // Store all OBU elements except the last one.
  for (int i = next_packet.first_obu; i <= next_packet.last_obu - 1; ++i) {
    const Obu& obu = obus_[i];
    size_t fragment_size = obu.size - obu_offset;
    write_at += WriteLeb128(fragment_size, write_at);
    if (obu_offset == 0) {
      *write_at++ = obu.header & ~kObuSizePresentBit;
    }
    if (obu_offset <= 1 && ObuHasExtension(obu.header)) {
      *write_at++ = obu.extension_header;
    }
    int payload_offset =
        std::max(0, obu_offset - (ObuHasExtension(obu.header) ? 2 : 1));
    size_t payload_size = obu.payload.size() - payload_offset;
    memcpy(write_at, obu.payload.data() + payload_offset, payload_size);
    write_at += payload_size;
    // All obus are stored from the beginning, except, may be, the first one.
    obu_offset = 0;
  }
  // Store the last OBU element.
  const Obu& last_obu = obus_[next_packet.last_obu];
  int fragment_size = next_packet.last_obu_size;
  RTC_DCHECK_GT(fragment_size, 0);
  if (NumObus(next_packet) > kMaxNumObusToOmitSize) {
    write_at += WriteLeb128(fragment_size, write_at);
  }
  if (obu_offset == 0 && fragment_size > 0) {
    *write_at++ = last_obu.header & ~kObuSizePresentBit;
    --fragment_size;
  }
  if (obu_offset <= 1 && ObuHasExtension(last_obu.header) &&
      fragment_size > 0) {
    *write_at++ = last_obu.extension_header;
    --fragment_size;
  }
  RTC_DCHECK_EQ(write_at - rtp_payload + fragment_size,
                kAggregationHeaderSize + next_packet.packet_size);
  int payload_offset =
      std::max(0, obu_offset - (ObuHasExtension(last_obu.header) ? 2 : 1));
  memcpy(write_at, last_obu.payload.data() + payload_offset, fragment_size);
  write_at += fragment_size;

  RTC_DCHECK_EQ(write_at - rtp_payload,
                kAggregationHeaderSize + next_packet.packet_size);

  ++packet_index_;
  if (packet_index_ == packets_.size()) {
    // TODO(danilchap): To support spatial scalability pass and use information
    // if this frame is the last in the temporal unit.
    packet->SetMarker(true);
  }
  return true;
}

}  // namespace webrtc
