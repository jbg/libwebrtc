/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/delta_encoding.h"

#include <limits>
#include <memory>  // TODO: !!! unique_ptr necessary?

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"

// TODO: !!! Make things unable to fail rather than keep supporting failure?

namespace webrtc {
namespace {

// Return the maximum integer of a given bit width.
// Examples:
// MaxOfBitWidth(1) = 0x01
// MaxOfBitWidth(8) = 0xff
// MaxOfBitWidth(32) = 0xffffffff
constexpr uint64_t ConstexprMaxOfBitWidth(size_t bit_width) {
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((1 << bit_width) - 1);
}

uint64_t MaxOfBitWidth(size_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_GE(bit_width, 64);
  return ConstexprMaxOfBitWidth(bit_width);
}

enum class EncodingType {
  kFixedSize = 0,
  kReserved1 = 1,
  kNumberOfEncodingTypes
};

constexpr size_t kBitsInHeaderForEncodingType = 2;
constexpr size_t kBitsInHeaderForOriginalWidthBits = 6;
constexpr size_t kBitsInHeaderForDeltaWidthBits = 6;
constexpr size_t kBitsInHeaderForSignedDeltas = 1;
constexpr size_t kBitsInHeaderForDeltasOptional = 1;

static_assert(static_cast<size_t>(EncodingType::kNumberOfEncodingTypes) <=
                  ConstexprMaxOfBitWidth(kBitsInHeaderForEncodingType),
              "Not all encoding types fit.");

class FixedLengthDeltaEncoder final : public DeltaEncoder {
 public:
  FixedLengthDeltaEncoder(uint8_t original_width_bits,
                          uint8_t delta_width_bits,
                          bool signed_deltas);
  ~FixedLengthDeltaEncoder() override = default;

  // TODO: !!! Prevent reuse.
  bool Encode(uint64_t base,
              std::vector<uint64_t> values,
              std::string* output) override;

 private:
  size_t EstimateOutputLengthBytes(size_t num_of_deltas) const;

  size_t EstimateHeaderLengthBits() const;

  size_t EstimateEncodedDeltasLengthBits(size_t num_of_deltas) const;

  void AllocateOutput(size_t num_of_deltas, std::string* output);

  bool Write(uint64_t val, size_t bit_count);

  bool EncodeHeader();

  bool EncodeDelta(uint64_t previous, uint64_t current);

  // TODO: !!!
  const uint8_t original_width_bits_;
  const uint64_t max_legal_value_;

  // TODO: !!!
  const uint8_t delta_width_bits_;
  const uint64_t max_legal_delta_;
  // TODO: !!! Min legal delta

  // TODO: !!!
  const bool signed_deltas_;

  // TODO: !!!
  const bool deltas_optional_;

  size_t written_bits_;

  // TODO: !!!
  std::unique_ptr<rtc::BitBufferWriter> buffer_;
};

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(uint8_t original_width_bits,
                                                 uint8_t delta_width_bits,
                                                 bool signed_deltas)
    : original_width_bits_(original_width_bits),
      max_legal_value_(MaxOfBitWidth(original_width_bits_)),
      delta_width_bits_(delta_width_bits),
      max_legal_delta_(MaxOfBitWidth(delta_width_bits)),
      signed_deltas_(signed_deltas),
      deltas_optional_(false),
      written_bits_(0) {
  RTC_DCHECK_GE(delta_width_bits_, 1);
  RTC_DCHECK_LE(delta_width_bits_, 64);
  RTC_DCHECK_GE(original_width_bits_, 1);
  RTC_DCHECK_LE(original_width_bits_, 64);
}

bool FixedLengthDeltaEncoder::Encode(uint64_t base,
                                     std::vector<uint64_t> values,
                                     std::string* output) {
  RTC_DCHECK(output);
  RTC_DCHECK(output->empty());
  RTC_DCHECK(!values.empty());

  AllocateOutput(values.size(), output);

  if (!EncodeHeader()) {
    output->clear();
    return false;
  }

  for (uint64_t value : values) {
    if (!EncodeDelta(base, value)) {
      output->clear();
      return false;
    }
    base = value;
  }

  return true;
}

size_t FixedLengthDeltaEncoder::EstimateOutputLengthBytes(
    size_t num_of_deltas) const {
  const size_t length_bits = EstimateHeaderLengthBits() +
                             EstimateEncodedDeltasLengthBits(num_of_deltas);
  return (length_bits / 8) + (length_bits % 8 > 0 ? 1 : 0);
}

size_t FixedLengthDeltaEncoder::EstimateHeaderLengthBits() const {
  return kBitsInHeaderForEncodingType + kBitsInHeaderForOriginalWidthBits +
         kBitsInHeaderForDeltaWidthBits + kBitsInHeaderForSignedDeltas +
         kBitsInHeaderForDeltasOptional;
}

size_t FixedLengthDeltaEncoder::EstimateEncodedDeltasLengthBits(
    size_t num_of_deltas) const {
  return num_of_deltas * (delta_width_bits_ + deltas_optional_);
}

void FixedLengthDeltaEncoder::AllocateOutput(size_t num_of_deltas,
                                             std::string* output) {
  RTC_DCHECK(output);
  RTC_DCHECK(output->empty());
  output->resize(EstimateOutputLengthBytes(num_of_deltas));
  uint8_t* fix_me = reinterpret_cast<uint8_t*>(&output[0]);  // TODO: !!!
  buffer_ = absl::make_unique<rtc::BitBufferWriter>(fix_me, output->size());
}

bool FixedLengthDeltaEncoder::Write(uint64_t val, size_t bit_count) {
  RTC_DCHECK(buffer_);
  if (buffer_->WriteBits(val, bit_count)) {
    written_bits_ += bit_count;
    return true;
  } else {
    buffer_.reset();
    written_bits_ = 0;
    return false;
  }
}

bool FixedLengthDeltaEncoder::EncodeHeader() {
  RTC_DCHECK(buffer_);
  RTC_DCHECK_EQ(written_bits_, 0u);

  return Write(static_cast<uint64_t>(EncodingType::kFixedSize),
               kBitsInHeaderForEncodingType) &&
         Write(static_cast<uint64_t>(original_width_bits_),
               kBitsInHeaderForOriginalWidthBits) &&
         Write(static_cast<uint64_t>(delta_width_bits_),
               kBitsInHeaderForDeltaWidthBits) &&
         Write(static_cast<uint64_t>(signed_deltas_),
               kBitsInHeaderForSignedDeltas) &&
         Write(static_cast<uint64_t>(deltas_optional_),
               kBitsInHeaderForDeltasOptional);
}

bool FixedLengthDeltaEncoder::EncodeDelta(uint64_t previous, uint64_t current) {
  RTC_DCHECK_LE(previous, max_legal_value_);
  RTC_DCHECK_LE(current, max_legal_value_);

  const uint64_t delta = current - previous;
  RTC_DCHECK_LE(delta, max_legal_delta_);

  RTC_DCHECK(buffer_);
  return Write(delta, delta_width_bits_);
}

}  // namespace
}  // namespace webrtc
