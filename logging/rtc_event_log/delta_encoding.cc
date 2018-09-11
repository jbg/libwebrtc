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

#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"

// TODO: !!! Make things unable to fail rather than keep supporting failure?

namespace webrtc {
namespace {

size_t BitsToBytes(size_t bits) {
  return (bits / 8) + (bits % 8 > 0 ? 1 : 0);
}

// TODO: !!! Difference between uint64_t and size_t throughout file.

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

// TODO: !!! Move? Delete?
class DeltaEncoder {
 public:
  virtual ~DeltaEncoder() = default;

  // TODO: !!! Fix
  // Encode |values| as a sequence of delta following on |base|, and writes it
  // into |output|. Return value indicates whether the operation was successful.
  // |base| is not guaranteed to be written into |output|, and must therefore
  // be provided separately to the decoder.
  // Encode() must write into |output| a bit pattern that would allow the
  // decoder to distinguish what type of DeltaEncoder produced it.
  virtual std::string Encode(uint64_t base, std::vector<uint64_t> values) = 0;
};

class FixedLengthDeltaEncoder final : public DeltaEncoder {
 public:
  FixedLengthDeltaEncoder(uint8_t original_width_bits,
                          uint8_t delta_width_bits,
                          bool signed_deltas,
                          size_t num_of_deltas);
  ~FixedLengthDeltaEncoder() override = default;

  // TODO: !!! Prevent reuse.
  std::string Encode(uint64_t base, std::vector<uint64_t> values) override;

 private:
  size_t EstimateOutputLengthBytes() const;

  size_t EstimateHeaderLengthBits() const;

  size_t EstimateEncodedDeltasLengthBits() const;

  bool Write(uint64_t val, size_t bit_count);

  bool EncodeHeader();

  bool EncodeDelta(uint64_t previous, uint64_t current);

  uint64_t ComputeDelta(uint64_t previous, uint64_t current) const;

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

  // TODO: !!!
  const size_t num_of_deltas_;

  // TODO: !!!
  std::string output_;
  rtc::BitBufferWriter bit_writer_;
  size_t written_bits_;
};

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(uint8_t original_width_bits,
                                                 uint8_t delta_width_bits,
                                                 bool signed_deltas,
                                                 size_t num_of_deltas)
    : original_width_bits_(original_width_bits),
      max_legal_value_(MaxOfBitWidth(original_width_bits_)),
      delta_width_bits_(delta_width_bits),
      max_legal_delta_(MaxOfBitWidth(delta_width_bits)),
      signed_deltas_(signed_deltas),
      deltas_optional_(false),
      num_of_deltas_(num_of_deltas),
      output_(EstimateOutputLengthBytes(), '\0'),
      // TODO: !!!
      bit_writer_(reinterpret_cast<uint8_t*>(&output_[0]), output_.size()),
      written_bits_(0) {
  RTC_DCHECK_GE(delta_width_bits_, 1);
  RTC_DCHECK_LE(delta_width_bits_, 64);
  RTC_DCHECK_GE(original_width_bits_, 1);
  RTC_DCHECK_LE(original_width_bits_, 64);
}

std::string FixedLengthDeltaEncoder::Encode(uint64_t base,
                                            std::vector<uint64_t> values) {
  RTC_DCHECK_EQ(written_bits_, 0u);
  RTC_DCHECK(!values.empty());

  if (!EncodeHeader()) {
    return std::string();
  }

  for (uint64_t value : values) {
    if (!EncodeDelta(base, value)) {
      return std::string();
    }
    base = value;
  }

  output_.resize(BitsToBytes(written_bits_));

  std::string result;
  std::swap(output_, result);
  written_bits_ = 0;
  return result;
}

size_t FixedLengthDeltaEncoder::EstimateOutputLengthBytes() const {
  const size_t length_bits =
      EstimateHeaderLengthBits() + EstimateEncodedDeltasLengthBits();
  return BitsToBytes(length_bits);
}

size_t FixedLengthDeltaEncoder::EstimateHeaderLengthBits() const {
  return kBitsInHeaderForEncodingType + kBitsInHeaderForOriginalWidthBits +
         kBitsInHeaderForDeltaWidthBits + kBitsInHeaderForSignedDeltas +
         kBitsInHeaderForDeltasOptional;
}

size_t FixedLengthDeltaEncoder::EstimateEncodedDeltasLengthBits() const {
  return num_of_deltas_ * (delta_width_bits_ + deltas_optional_);
}

bool FixedLengthDeltaEncoder::Write(uint64_t val, size_t bit_count) {
  if (bit_writer_.WriteBits(val, bit_count)) {
    written_bits_ += bit_count;
    return true;
  } else {
    written_bits_ = 0;
    return false;
  }
}

bool FixedLengthDeltaEncoder::EncodeHeader() {
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
  return Write(ComputeDelta(previous, current), delta_width_bits_);
}

uint64_t FixedLengthDeltaEncoder::ComputeDelta(uint64_t previous,
                                               uint64_t current) const {
  RTC_DCHECK_LE(previous, max_legal_value_);
  RTC_DCHECK_LE(current, max_legal_value_);

  uint64_t delta;
  if (current >= previous) {
    // Simply "walk" forward.
    delta = current - previous;
  } else {  // previous > current
    // "Walk" until the max value, one more step to 0, then to |current|.
    delta = (max_legal_value_ - previous) + 1 + current;
  }

  RTC_DCHECK_LE(delta, max_legal_delta_);
  return delta;
}

// TODO: !!!
// std::unique_ptr<DeltaEncoder> Get

}  // namespace

std::string EncodeDeltas(uint64_t base, std::vector<uint64_t> values) {
  // TODO: !!!
  return std::string();
}

// TODO: !!! Document
std::vector<uint64_t> DecodeDeltas(const std::string input,
                                   uint64_t base,
                                   size_t num_of_deltas) {
  return std::vector<uint64_t>();  // TODO: !!!
}

}  // namespace webrtc
