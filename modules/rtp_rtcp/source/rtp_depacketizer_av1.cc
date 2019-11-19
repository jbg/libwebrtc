/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"

#include <stddef.h>
#include <stdint.h>

#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// AV1 format:
//
// RTP payload syntax:
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |Z|Y| W |-|-|-|-| (REQUIRED)
//    +=+=+=+=+=+=+=+=+ (REPEATED W-1 times, or any times if W = 0)
//    |1|             |
//    +-+ OBU fragment|
//    |1|             | (REQUIRED, leb128 encoded)
//    +-+    size     |
//    |0|             |
//    +-+-+-+-+-+-+-+-+
//    |  OBU fragment |
//    |     ...       |
//    +=+=+=+=+=+=+=+=+
//    |     ...       |
//    +=+=+=+=+=+=+=+=+ if W > 0, last fragment MUST NOT have size field
//    |  OBU fragment |
//    |     ...       |
//    +=+=+=+=+=+=+=+=+
//
//
// OBU syntax:
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |0| type  |X|S|-| (REQUIRED)
//    +-+-+-+-+-+-+-+-+
// X: | TID |SID|-|-|-| (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
//    |1|             |
//    +-+ OBU payload |
// S: |1|             | (OPTIONAL, variable length leb128 encoded)
//    +-+    size     |
//    |0|             |
//    +-+-+-+-+-+-+-+-+
//    |  OBU payload  |
//    |     ...       |
class ArrayOfArrayViews {
 public:
  class const_iterator;
  ArrayOfArrayViews() = default;
  ArrayOfArrayViews(const ArrayOfArrayViews&) = default;
  ArrayOfArrayViews& operator=(const ArrayOfArrayViews&) = default;
  ~ArrayOfArrayViews() = default;

  const_iterator begin() const;
  bool IsEnd(const const_iterator&) const;
  bool empty() const { return data_.empty(); }
  size_t size() const { return size_; }
  void CopyTo(rtc::ArrayView<uint8_t> destination, const_iterator first) const;

  void Add(const uint8_t* data, size_t size) {
    data_.emplace_back(data, size);
    size_ += size;
  }

 private:
  using Storage = absl::InlinedVector<rtc::ArrayView<const uint8_t>, 2>;

  size_t size_ = 0;
  Storage data_;
};

class ArrayOfArrayViews::const_iterator {
 public:
  const_iterator() = default;
  const_iterator(ArrayOfArrayViews::Storage::const_iterator outer, size_t inner)
      : outer_(outer), inner_(inner) {}
  const_iterator(const const_iterator&) = default;
  const_iterator& operator=(const const_iterator&) = default;

  const_iterator& operator++() {
    if (++inner_ == outer_->size()) {
      ++outer_;
      inner_ = 0;
    }
    return *this;
  }
  uint8_t operator*() const { return (*outer_)[inner_]; }

 private:
  friend ArrayOfArrayViews;
  Storage::const_iterator outer_;
  size_t inner_;
};

ArrayOfArrayViews::const_iterator ArrayOfArrayViews::begin() const {
  return const_iterator(data_.begin(), 0);
}

bool ArrayOfArrayViews::IsEnd(const const_iterator& it) const {
  return it.outer_ == data_.end();
}

void ArrayOfArrayViews::CopyTo(rtc::ArrayView<uint8_t> destination,
                               const_iterator first) const {
  if (IsEnd(first)) {
    // Empty payload. That is valid, e.g. Temporal Delimiters are always empty.
    RTC_DCHECK_EQ(destination.size(), 0);
    return;
  }
  uint8_t* to = destination.data();
  uint8_t* const to_end = destination.data() + destination.size();
  size_t first_chunk_size = first.outer_->size() - first.inner_;
  RTC_DCHECK_LE(first_chunk_size, to_end - to);
  memcpy(to, first.outer_->data() + first.inner_, first_chunk_size);
  to += first_chunk_size;
  for (auto it = std::next(first.outer_); it != data_.end(); ++it) {
    RTC_DCHECK_LE(it->size(), to_end - to);
    memcpy(to, it->data(), it->size());
    to += it->size();
  }
  RTC_DCHECK_EQ(to_end - to, 0);
}

struct ObuInfo {
  // Size of the obu_header and obu_size fields in the ouput frame.
  size_t prefix_size = 0;
  // Size of the obu payload in the output frame, i.e. excluding header
  size_t payload_size = 0;
  // leb128 encoded payload_size.
  uint8_t encoded_payload_size[8];
  // iterator pointing to the beginning of the obu payload.
  ArrayOfArrayViews::const_iterator payload_offset;
  // OBU payloads as written in the rtp packet payloads.
  ArrayOfArrayViews data;
};
// Expect that majority of the frame won't use more than 4 obus.
// In a simple stream delta frame consist of single Frame OBU, while key frame
// also has Sequence Header OBU.
using VectorObuInfo = absl::InlinedVector<ObuInfo, 4>;

constexpr int kObuTypeSequenceHeader = 1;
constexpr uint8_t kObuHasSizeBit = 0b0'0000'010;

bool ObuHasExtension(uint8_t obu_header) {
  return obu_header & 0b0'0000'100u;
}

bool ObuHasSize(uint8_t obu_header) {
  return obu_header & kObuHasSizeBit;
}

int ObuType(uint8_t obu_header) {
  return (obu_header & 0b0'1111'000u) >> 3;
}

bool RtpStartsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b1000'0000u;
}
bool RtpEndsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b0100'0000u;
}
int RtpNumObus(uint8_t aggregation_header) {  // 0 for any number of obus.
  return (aggregation_header & 0b0011'0000u) >> 4;
}

// Reorgonizes array of rtp payloads into array of obus:
// fills ObuInfo::data field.
VectorObuInfo ParseObus(
    rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads) {
  VectorObuInfo obus;
  bool expect_continues_obu = false;
  for (rtc::ArrayView<const uint8_t> rtp_payload : rtp_payloads) {
    rtc::ByteBufferReader payload(
        reinterpret_cast<const char*>(rtp_payload.data()), rtp_payload.size());
    uint8_t aggregation_header;
    if (!payload.ReadUInt8(&aggregation_header)) {
      RTC_DLOG(WARNING) << "Failed to find aggregation header in the packet";
      return {};
    }
    // Z-bit: 1 if the first OBU contained in the packet is a continuation of a
    // previous OBU.
    bool continues_obu = RtpStartsWithFragment(aggregation_header);
    if (continues_obu != expect_continues_obu) {
      RTC_DLOG(WARNING) << "Unexpected Z-bit " << continues_obu;
      return {};
    }
    if (!continues_obu) {
      obus.emplace_back();
    }
    int num_expected_obus = RtpNumObus(aggregation_header);
    for (int obu_index = 1; payload.Length() > 0; ++obu_index) {
      ObuInfo& obu = obu_index == 1 ? obus.back() : obus.emplace_back();
      uint64_t fragment_size;
      // When num_expected_obus > 0, last OBU (fragment) is not preceeded by
      // the size field. See W field in
      // https://aomediacodec.github.io/av1-rtp-spec/#43-av1-aggregation-header
      bool has_fragment_size = (obu_index != num_expected_obus);
      if (has_fragment_size) {
        if (!payload.ReadUVarint(&fragment_size)) {
          RTC_DLOG(WARNING) << "Failed to reader fragment size.";
          return {};
        }
        if (fragment_size > payload.Length()) {
          // Malformed input: written size is larger than remaining buffer.
          RTC_DLOG(WARNING) << "Malformed fragment size " << fragment_size
                            << " is larger than remaining size "
                            << payload.Length() << " while reading obu #"
                            << obu_index << "/" << num_expected_obus;
          return {};
        }
      } else {
        fragment_size = payload.Length();
      }
      // While it is in-practical to pass empty fragments, it is still possible.
      if (fragment_size > 0) {
        obu.data.Add(reinterpret_cast<const uint8_t*>(payload.Data()),
                     fragment_size);
        payload.Consume(fragment_size);
      }
    }
    // Z flag should be same as Y flag of the next packet.
    expect_continues_obu = RtpEndsWithFragment(aggregation_header);
  }
  if (expect_continues_obu) {
    RTC_DLOG(WARNING) << "Last packet shouldn't have last obu fragmented.";
    return {};
  }
  return obus;
}

// Returns number of bytes consumed.
int WriteLeb128(size_t value, uint8_t* buffer) {
  RTC_DCHECK_LT(value, size_t{1} << 32);
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

// Calculates sizes for the Obu:
//  Changes payload_size from the size of all fragments into the size of obu
//  payload. Extracts and validates obu size if present. Finds offset for the
//  obu payload. Calculates total_size obu would occupy in the frame.
// Returns false if obu found to be misformed.
bool CalculateObuSizes(ObuInfo* obu) {
  if (obu->data.empty()) {
    RTC_DLOG(WARNING) << "Invalid bitstream: empty obu provided.";
    return false;
  }
  auto it = obu->data.begin();
  uint8_t obu_header = *it;
  ++it;
  if (!ObuHasExtension(obu_header)) {
    obu->prefix_size = 1;
  } else {
    obu->prefix_size = 2;
    if (obu->data.IsEnd(it)) {
      return false;
    }
    ++it;
  }
  // Read, validate, and skip size, if present.
  if (!ObuHasSize(obu_header)) {
    obu->payload_size = obu->data.size() - obu->prefix_size;
  } else {
    uint64_t signaled_size = 0;
    int size_size = 0;
    uint8_t byte;
    do {
      if (obu->data.IsEnd(it) || size_size >= 8) {
        RTC_DLOG(WARNING) << "Failed to read obu_size. Size field is too long: "
                          << size_size << " bytes processed.";
        return false;
      }
      byte = *it;
      signaled_size |= (byte & 0x7F) << (size_size * 7);
      ++size_size;
      ++it;
    } while ((byte & 0x80) != 0);

    obu->payload_size = obu->data.size() - obu->prefix_size - size_size;
    if (signaled_size != obu->payload_size) {
      // obu_size was present in the bitstream and mismatches calculated size.
      RTC_DLOG(WARNING) << "Mismatch in obu_size. signaled: " << signaled_size
                        << ", actual: " << obu->payload_size;
      return false;
    }
  }
  // At this point obu->data should never change since, so it should be ok
  // to store iterator into it.
  obu->payload_offset = it;
  obu->prefix_size += WriteLeb128(obu->payload_size, obu->encoded_payload_size);
  return true;
}

// Writes obu into raw buffer.
void StoreObu(const ObuInfo& obu, uint8_t* write_at) {
  const uint8_t* const end_write_at =
      write_at + obu.prefix_size + obu.payload_size;
  auto it = obu.data.begin();
  uint8_t obu_header = *it;
  *write_at = obu_header | kObuHasSizeBit;
  ++write_at;
  int size_size = obu.prefix_size - 1;
  if (ObuHasExtension(obu_header)) {
    *write_at = *++it;  // extension_header
    ++write_at;
    --size_size;
  }
  // Copy the obu_size field.
  memcpy(write_at, obu.encoded_payload_size, size_size);
  write_at += size_size;
  // Copy the payload.
  obu.data.CopyTo(rtc::MakeArrayView(write_at, end_write_at - write_at),
                  obu.payload_offset);
}

}  // namespace

rtc::scoped_refptr<EncodedImageBuffer> RtpDepacketizerAv1::AssembleFrame(
    rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads) {
  VectorObuInfo obus = ParseObus(rtp_payloads);
  if (obus.empty()) {
    return nullptr;
  }

  size_t frame_size = 0;
  for (ObuInfo& obu : obus) {
    if (!CalculateObuSizes(&obu)) {
      return nullptr;
    }
    frame_size += (obu.prefix_size + obu.payload_size);
  }

  rtc::scoped_refptr<EncodedImageBuffer> bitstream =
      EncodedImageBuffer::Create(frame_size);
  uint8_t* write_at = bitstream->data();
  for (const ObuInfo& obu : obus) {
    StoreObu(obu, write_at);
    write_at += (obu.prefix_size + obu.payload_size);
  }
  RTC_CHECK_EQ(write_at - bitstream->data(), bitstream->size());
  return bitstream;
}

bool RtpDepacketizerAv1::Parse(ParsedPayload* parsed_payload,
                               const uint8_t* payload_data,
                               size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0) {
    RTC_DLOG(LS_ERROR) << "Empty rtp payload.";
    return false;
  }
  // To assemble frame, all of the rtp payload is required, including
  // aggregation header.
  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;

  rtc::ByteBufferReader payload(reinterpret_cast<const char*>(payload_data),
                                payload_data_length);
  uint8_t aggregation_header;
  RTC_CHECK(payload.ReadUInt8(&aggregation_header));

  // TODO(danilchap): Set AV1 codec when there is such enum value
  parsed_payload->video.codec = VideoCodecType::kVideoCodecGeneric;
  // These are not accurate since frame may consist of several packet aligned
  // chunks of obus, but should be good enough for most cases. It might produce
  // frame that do not map to any real frame, but av1 decoder should be able to
  // handle it since it promise to handle individual obus rather than full
  // frames.
  parsed_payload->video.is_first_packet_in_frame =
      !RtpStartsWithFragment(aggregation_header);
  parsed_payload->video.is_last_packet_in_frame =
      !RtpEndsWithFragment(aggregation_header);
  parsed_payload->video.frame_type = VideoFrameType::kVideoFrameDelta;
  // If packet starts a frame, check if it contains Sequence Header OBU.
  // In that case treat it as key frame packet.
  if (parsed_payload->video.is_first_packet_in_frame) {
    int num_expected_obus = RtpNumObus(aggregation_header);

    // The only OBU that can preceed SequenceHeader is a TemporalDelimiter OBU,
    // so check no more than two OBUs while searching for SH.
    for (int obu_index = 1; payload.Length() > 0 && obu_index <= 2;
         ++obu_index) {
      uint64_t fragment_size;
      // When num_expected_obus > 0, last OBU (fragment) is not preceeded by
      // the size field. See W field in
      // https://aomediacodec.github.io/av1-rtp-spec/#43-av1-aggregation-header
      bool has_fragment_size = (obu_index != num_expected_obus);
      if (has_fragment_size) {
        if (!payload.ReadUVarint(&fragment_size)) {
          RTC_DLOG(LS_WARNING)
              << "Failed to read OBU fragment size for OBU#" << obu_index;
          return false;
        }
        if (fragment_size > payload.Length()) {
          RTC_DLOG(LS_WARNING) << "OBU fragment size " << fragment_size
                               << " exceeds remaining payload size "
                               << payload.Length() << " for OBU#" << obu_index;
          // Malformed input: written size is larger than remaining buffer.
          return false;
        }
      } else {
        fragment_size = payload.Length();
      }
      // Though it is inpractical to pass empty fragments, it is allowed.
      if (fragment_size == 0) {
        RTC_LOG(LS_WARNING)
            << "Weird obu of size 0 at offset "
            << (payload_data_length - payload.Length()) << ", skipping.";
        continue;
      }
      uint8_t obu_header = *reinterpret_cast<const uint8_t*>(payload.Data());
      if (ObuType(obu_header) == kObuTypeSequenceHeader) {
        // TODO(bugs.webrtc.org/11042): Check frame_header OBU and/or frame OBU
        // too for other conditions of the start of a new coded video sequence.
        // For proper checks checking single packet might not be enough. See
        // https://aomediacodec.github.io/av1-spec/av1-spec.pdf section 7.5
        parsed_payload->video.frame_type = VideoFrameType::kVideoFrameKey;
        break;
      }
      payload.Consume(fragment_size);
    }
  }

  return true;
}

}  // namespace webrtc
