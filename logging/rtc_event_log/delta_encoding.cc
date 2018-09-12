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

#include <algorithm>
#include <limits>
#include <memory>  // TODO: !!! unique_ptr necessary?

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

// TODO: !!! Make things unable to fail rather than keep supporting failure?

namespace webrtc {
namespace {

size_t BitsToBytes(size_t bits) {
  return (bits / 8) + (bits % 8 > 0 ? 1 : 0);
}

// TODO(eladalon): Replace by something more efficient.
uint64_t BitWidth(uint64_t input) {
  uint64_t width = 0;
  do {  // input == 0 -> width == 1
    width += 1;
    input >>= 1;
  } while (input != 0);
  return width;
}

uint64_t MaxBitWidth(const std::vector<uint64_t>& inputs) {
  RTC_DCHECK(!inputs.empty());
  // The biggest element will also be the widest, so we can spare ourselves
  // the effort of applying BitWidth to each element.
  return BitWidth(*std::max_element(inputs.begin(), inputs.end()));
}

uint64_t MaxDeltaBitWidthWithGivenOriginalValueWidth(
    const std::vector<uint64_t>& inputs,
    uint64_t original_width_bits) {
  return 64;  // TODO: !!!
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

// Extends BitBufferWriter by (1) keeping track of the number of bits written
// and (2) owning its buffer.
class BitWriter {
 public:
  BitWriter(size_t byte_count)
      : buffer_(byte_count, '\0'),
        bit_writer_(reinterpret_cast<uint8_t*>(&buffer_[0]), buffer_.size()),
        written_bits_(0),
        valid_(true) {
    RTC_DCHECK_GT(byte_count, 0);
  }

  bool WriteBits(uint64_t val, size_t bit_count) {
    RTC_DCHECK(valid_);
    return bit_writer_.WriteBits(val, bit_count);
  }

  std::string GetString() {
    RTC_DCHECK(valid_);
    valid_ = false;

    buffer_.resize(BitsToBytes(written_bits_));

    std::string result;
    std::swap(buffer_, result);
    return result;
  }

 private:
  std::string buffer_;
  rtc::BitBufferWriter bit_writer_;
  // Note: Counting bits instead of bytes wrap around earlier than it has to,
  // which means the maximum length is lower than it must be. We don't expect
  // to go anywhere near the limit, though, so this is good enough.
  size_t written_bits_;
  bool valid_;
};

// // TODO: !!! Move? Delete?
// class DeltaEncoder {
//  public:
//   virtual ~DeltaEncoder() = default;

//   // TODO: !!! Fix
//   // Encode |values| as a sequence of delta following on |base|, and writes
//   it
//   // into |output|. Return value indicates whether the operation was
//   successful.
//   // |base| is not guaranteed to be written into |output|, and must therefore
//   // be provided separately to the decoder.
//   // Encode() must write into |output| a bit pattern that would allow the
//   // decoder to distinguish what type of DeltaEncoder produced it.
//   virtual std::string Encode(uint64_t base,
//                              const std::vector<uint64_t>& values) = 0;
// };

class FixedLengthDeltaEncoder final {
 public:
  // TODO: !!!
  static std::string EncodeDeltas(uint64_t base,
                                  const std::vector<uint64_t>& values);

 private:
  FixedLengthDeltaEncoder(uint64_t original_width_bits,
                          uint64_t delta_width_bits,
                          bool signed_deltas,
                          bool deltas_optional);

  // TODO: !!! Prevent reuse.
  std::string Encode(uint64_t base,
                     const std::vector<uint64_t>& values) /*override*/;

  size_t EstimateOutputLengthBytes(uint64_t num_of_deltas) const;

  size_t EstimateHeaderLengthBits() const;

  size_t EstimateEncodedDeltasLengthBits(uint64_t num_of_deltas) const;

  bool EncodeHeader(BitWriter* writer);

  bool EncodeDelta(BitWriter* writer, uint64_t previous, uint64_t current);

  uint64_t ComputeDelta(uint64_t previous, uint64_t current) const;

  // TODO: !!!
  const uint64_t original_width_bits_;
  const uint64_t max_legal_value_;

  // TODO: !!!
  const uint64_t delta_width_bits_;
  const uint64_t max_legal_delta_;
  // TODO: !!! Min legal delta

  // TODO: !!!
  const bool signed_deltas_;

  // TODO: !!!
  const bool deltas_optional_;
};

std::string FixedLengthDeltaEncoder::EncodeDeltas(
    uint64_t base,
    const std::vector<uint64_t>& values) {
  RTC_DCHECK(!values.empty());

  const uint64_t original_width_bits = MaxBitWidth(values);

  std::vector<uint64_t> deltas(values.size());
  deltas[0] = values[0] - base;
  for (size_t i = 1; i < deltas.size(); ++i) {
    deltas[i] = values[i] - values[i - 1];
  }

  const uint64_t delta_width_bits_unsigned = MaxBitWidth(deltas);
  const uint64_t delta_width_bits_signed =
      MaxDeltaBitWidthWithGivenOriginalValueWidth(deltas, original_width_bits);

  // Note: Preference for unsigned if the two have the same width (efficiency).
  const bool signed_deltas =
      delta_width_bits_signed > delta_width_bits_unsigned;
  const uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  // TODO: !!! I had misunderstood the optional field; it is actually
  // deterministic; we either have at least one, or none.
  std::string encoding;
  for (bool deltas_optional : {false, true}) {
    FixedLengthDeltaEncoder encoder(original_width_bits, delta_width_bits,
                                    signed_deltas, deltas_optional);
    std::string potential_encoding = encoder.Encode(base, values);
    RTC_DCHECK(!potential_encoding.empty());  // TODO: !!!
    if (encoding.empty() || encoding.size() > potential_encoding.size()) {
      std::swap(encoding, potential_encoding);
    }
  }

  return encoding;
}

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(uint64_t original_width_bits,
                                                 uint64_t delta_width_bits,
                                                 bool signed_deltas,
                                                 bool deltas_optional)
    : original_width_bits_(original_width_bits),
      max_legal_value_(MaxOfBitWidth(original_width_bits_)),
      delta_width_bits_(delta_width_bits),
      max_legal_delta_(MaxOfBitWidth(delta_width_bits)),
      signed_deltas_(signed_deltas),
      deltas_optional_(deltas_optional) {
  RTC_DCHECK_GE(delta_width_bits_, 1);
  RTC_DCHECK_LE(delta_width_bits_, 64);
  RTC_DCHECK_GE(original_width_bits_, 1);
  RTC_DCHECK_LE(original_width_bits_, 64);
  RTC_DCHECK_LE(delta_width_bits_, original_width_bits_);
}

std::string FixedLengthDeltaEncoder::Encode(
    uint64_t base,
    const std::vector<uint64_t>& values) {
  RTC_DCHECK(!values.empty());

  // TODO: !!! Move here after all?
  auto writer =
      absl::make_unique<BitWriter>(EstimateOutputLengthBytes(values.size()));

  if (!EncodeHeader(writer.get())) {  // TODO: !!! DCHECK?
    return std::string();
  }

  for (uint64_t value : values) {
    if (!EncodeDelta(writer.get(), base, value)) {
      return std::string();  // TODO: !!! DCHECK?
    }
    base = value;
  }

  return writer->GetString();
}

// TODO: !!! It might be that the deltas are even smaller than the values.
size_t FixedLengthDeltaEncoder::EstimateOutputLengthBytes(
    uint64_t num_of_deltas) const {
  const size_t length_bits = EstimateHeaderLengthBits() +
                             EstimateEncodedDeltasLengthBits(num_of_deltas);
  return BitsToBytes(length_bits);
}

size_t FixedLengthDeltaEncoder::EstimateHeaderLengthBits() const {
  return kBitsInHeaderForEncodingType + kBitsInHeaderForOriginalWidthBits +
         kBitsInHeaderForDeltaWidthBits + kBitsInHeaderForSignedDeltas +
         kBitsInHeaderForDeltasOptional;
}

size_t FixedLengthDeltaEncoder::EstimateEncodedDeltasLengthBits(
    uint64_t num_of_deltas) const {
  return num_of_deltas * (delta_width_bits_ + deltas_optional_);
}

bool FixedLengthDeltaEncoder::EncodeHeader(BitWriter* writer) {
  RTC_DCHECK(writer);
  return writer->WriteBits(static_cast<uint64_t>(EncodingType::kFixedSize),
                           kBitsInHeaderForEncodingType) &&
         writer->WriteBits(original_width_bits_,
                           kBitsInHeaderForOriginalWidthBits) &&
         writer->WriteBits(delta_width_bits_, kBitsInHeaderForDeltaWidthBits) &&
         writer->WriteBits(static_cast<uint64_t>(signed_deltas_),
                           kBitsInHeaderForSignedDeltas) &&
         writer->WriteBits(static_cast<uint64_t>(deltas_optional_),
                           kBitsInHeaderForDeltasOptional);
}

bool FixedLengthDeltaEncoder::EncodeDelta(BitWriter* writer,
                                          uint64_t previous,
                                          uint64_t current) {
  RTC_DCHECK(writer);
  return writer->WriteBits(ComputeDelta(previous, current), delta_width_bits_);
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

class FixedLengthDeltaDecoder final {
 public:
  // Checks whether FixedLengthDeltaDecoder is a suitable decoder for this
  // bitstream. Note that this does not necessarily mean that the stream is
  // not defective; decoding might still fail later.
  static bool IsSuitableDecoderFor(const std::string& input);

  // TODO: !!!
  static std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                            uint64_t base,
                                            size_t num_of_deltas);

 private:
  // TODO: !!!
  static std::unique_ptr<FixedLengthDeltaDecoder>
  Create(const std::string& input, uint64_t base, size_t num_of_deltas);

  FixedLengthDeltaDecoder(std::unique_ptr<rtc::BitBuffer> reader,
                          uint64_t base,
                          size_t num_of_deltas);

  std::vector<uint64_t> Decode();

  std::unique_ptr<rtc::BitBuffer> reader_;
  const uint64_t base_ __attribute__((unused));
  const size_t num_of_deltas_ __attribute__((unused));
};

bool FixedLengthDeltaDecoder::IsSuitableDecoderFor(const std::string& input) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return false;
  }

  rtc::BitBuffer reader(reinterpret_cast<const uint8_t*>(&input[0]),
                        kBitsInHeaderForEncodingType);

  uint32_t encoding_type_bits;
  const bool result =
      reader.ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);

  return static_cast<EncodingType>(encoding_type_bits) ==
         EncodingType::kFixedSize;
}

std::vector<uint64_t> FixedLengthDeltaDecoder::DecodeDeltas(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  RTC_DCHECK(IsSuitableDecoderFor(input));

  auto decoder = FixedLengthDeltaDecoder::Create(input, base, num_of_deltas);
  if (!decoder) {
    return std::vector<uint64_t>();
  }

  return decoder->Decode();
}

std::unique_ptr<FixedLengthDeltaDecoder> FixedLengthDeltaDecoder::Create(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return nullptr;
  }

  auto reader = absl::make_unique<rtc::BitBuffer>(
      reinterpret_cast<const uint8_t*>(&input[0]), input.length());

  uint32_t encoding_type_bits;
  const bool result =
      reader->ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);

  const EncodingType encoding = static_cast<EncodingType>(encoding_type_bits);
  if (encoding != EncodingType::kFixedSize) {
    return nullptr;
  }

  uint32_t original_width_bits;
  uint32_t delta_width_bits;
  uint32_t signed_deltas;
  uint32_t deltas_optional;
  if (!reader->ReadBits(&original_width_bits,
                        kBitsInHeaderForOriginalWidthBits) ||
      !reader->ReadBits(&delta_width_bits, kBitsInHeaderForDeltaWidthBits) ||
      !reader->ReadBits(&signed_deltas, kBitsInHeaderForSignedDeltas) ||
      !reader->ReadBits(&deltas_optional, kBitsInHeaderForDeltasOptional)) {
    return nullptr;
  }

  return absl::WrapUnique(
      new FixedLengthDeltaDecoder(std::move(reader), base, num_of_deltas));
}

FixedLengthDeltaDecoder::FixedLengthDeltaDecoder(
    std::unique_ptr<rtc::BitBuffer> reader,
    uint64_t base,
    size_t num_of_deltas)
    : reader_(std::move(reader)), base_(base), num_of_deltas_(num_of_deltas) {
  // TODO: !!!
}

std::vector<uint64_t> FixedLengthDeltaDecoder::Decode() {
  std::vector<uint64_t> values(num_of_deltas_);

  return std::vector<uint64_t>();
}

}  // namespace

std::string EncodeDeltas(uint64_t base, const std::vector<uint64_t>& values) {
  // TODO(eladalon): Support additional encodings.
  return FixedLengthDeltaEncoder::EncodeDeltas(base, values);
}

// TODO: !!! Document
std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                   uint64_t base,
                                   size_t num_of_deltas) {
  RTC_DCHECK_GT(num_of_deltas, 0);

  if (FixedLengthDeltaDecoder::IsSuitableDecoderFor(input)) {
    return FixedLengthDeltaDecoder::DecodeDeltas(input, base, num_of_deltas);
  }

  RTC_LOG(LS_WARNING) << "Could not decode delta-encoded stream.";
  return std::vector<uint64_t>();
}

}  // namespace webrtc
