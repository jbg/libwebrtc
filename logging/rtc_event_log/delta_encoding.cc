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
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

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

uint64_t MaxSignedDeltaBitWidth(const std::vector<uint64_t>& inputs,
                                uint64_t original_width_bits) {
  // This prevent the use of signed deltas, by always assuming
  // they will not provide value over unsigned.
  // TODO(eladalon): Add support for signed deltas.
  return 64;
}

// TODO: !!! Difference between uint64_t and size_t throughout file.

// Return the maximum integer of a given bit width.
// Examples:
// MaxValueOfBitWidth(1) = 0x01
// MaxValueOfBitWidth(6) = 0x3f
// MaxValueOfBitWidth(8) = 0xff
// MaxValueOfBitWidth(32) = 0xffffffff
constexpr uint64_t ConstexprMaxValueOfBitWidth(size_t bit_width) {
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((1 << bit_width) - 1);
}

uint64_t MaxValueOfBitWidth(size_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_LE(bit_width, 64);
  return ConstexprMaxValueOfBitWidth(bit_width);
}

// Computes the delta between |previous| and |current|, under the assumption
// that wrap-around occurs after |bit_width| is exceeded.
uint64_t ComputeDelta(uint64_t previous, uint64_t current, uint64_t width) {
  RTC_DCHECK(width == 64 || current < (1 << width));
  RTC_DCHECK(width == 64 || previous < (1 << width));

  if (current >= previous) {
    // Simply "walk" forward.
    return current - previous;
  } else {  // previous > current
    // "Walk" until the max value, one more step to 0, then to |current|.
    return (MaxValueOfBitWidth(width) - previous) + 1 + current;
  }
}

// TODO: !!! Explain.
enum class EncodingType {
  kFixedSizeWithOnlyMandatoryFields = 0,
  kFixedSizeWithAllOptionalFields = 1,
  kReserved1 = 2,
  kReserved2 = 3,
  kNumberOfEncodingTypes  // Keep last
};

// The width of each field in the encoding header. Note that this is the
// width in case the field exists; not all fields occur in all encoding types.
constexpr size_t kBitsInHeaderForEncodingType = 2;
constexpr size_t kBitsInHeaderForOriginalWidthBits = 6;
constexpr size_t kBitsInHeaderForDeltaWidthBits = 6;
constexpr size_t kBitsInHeaderForSignedDeltas = 1;
constexpr size_t kBitsInHeaderForDeltasOptional = 1;

// Default values for when the encoding header does not specify explicitly.
constexpr uint64_t kDefaultOriginalWidthBits = 64;
constexpr bool kDefaultSignedDeltas = false;
constexpr bool kDefaultDeltasOptional = false;

static_assert(static_cast<size_t>(EncodingType::kNumberOfEncodingTypes) <=
                  ConstexprMaxValueOfBitWidth(kBitsInHeaderForEncodingType) + 1,
              "Not all encoding types fit.");

// Wrap BitBufferWriter and extend its functionality by (1) keeping track of
//  the number of bits written and (2) owning its buffer.
class BitWriter final {
 public:
  BitWriter(size_t byte_count)
      : buffer_(byte_count, '\0'),
        bit_writer_(reinterpret_cast<uint8_t*>(&buffer_[0]), buffer_.size()),
        written_bits_(0),
        valid_(true) {
    RTC_DCHECK_GT(byte_count, 0);
  }

  // TODO: !!! Make sure the allocation never messes up.
  void WriteBits(uint64_t val, size_t bit_count) {
    RTC_DCHECK(valid_);
    const bool success = bit_writer_.WriteBits(val, bit_count);
    RTC_DCHECK(success);
    written_bits_ += bit_count;
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

  RTC_DISALLOW_COPY_AND_ASSIGN(BitWriter);
};

class FixedLengthDeltaEncoder final {
 public:
  // See webrtc::EncodeDeltas() for general details.
  // This function must write into |output| a bit pattern that would allow the
  // decoder to determine whether it was produced by FixedLengthDeltaEncoder,
  // and can therefore be decoded by FixedLengthDeltaDecoder, or whether it
  // was produced by a different encoder.
  static std::string EncodeDeltas(uint64_t base,
                                  const std::vector<uint64_t>& values);

 private:
  FixedLengthDeltaEncoder(uint64_t original_width_bits,
                          uint64_t delta_width_bits,
                          bool signed_deltas,
                          bool deltas_optional);

  std::string Encode(uint64_t base, const std::vector<uint64_t>& values);

  size_t LowerBoundOutputLengthBytes(uint64_t num_of_deltas) const;
  size_t LowerBoundHeaderLengthBits() const;
  size_t LowerBoundEncodedDeltasLengthBits(uint64_t num_of_deltas) const;

  // Encode the compression parameters into the stream.
  void EncodeHeader(BitWriter* writer);

  // Encode a given delta into the stream.
  void EncodeDelta(BitWriter* writer, uint64_t previous, uint64_t current);

  // TODO: !!! Reorder.

  // Number of bits necessary to hold the largest value in the sequence of
  // values that |this| will be used to encode.
  const uint64_t original_width_bits_;

  // Number of bits necessary to hold the widest(*) of the deltas between the
  // values |this| will be used to encode.
  // (*) - Widest might not be the largest, if signed deltas are used.
  const uint64_t delta_width_bits_;

  // Whether deltas are signed.
  // TODO(eladalon): Add support for signed deltas.
  const bool signed_deltas_;

  // Whether the values encoded by |this| are optional. That is, it may be
  // that some of them might have to be non-existent rather than assume
  // a value. (Do not confuse value 0 with non-existence; the two are distinct).
  // TODO(eladalon): Add support for optional elements.
  const bool deltas_optional_;  // TODO: !!! Rename.

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaEncoder);
};

std::string FixedLengthDeltaEncoder::EncodeDeltas(
    uint64_t base,
    const std::vector<uint64_t>& values) {
  RTC_DCHECK(!values.empty());

  const uint64_t original_width_bits =
      std::max(BitWidth(base),
               BitWidth(*std::max_element(values.begin(), values.end())));

  std::vector<uint64_t> deltas(values.size());
  deltas[0] = ComputeDelta(base, values[0], original_width_bits);
  uint64_t max_delta = deltas[0];
  for (size_t i = 1; i < deltas.size(); ++i) {
    deltas[i] = ComputeDelta(values[i - 1], values[i], original_width_bits);
    max_delta = std::max(deltas[i], max_delta);
  }

  // We indicate the special case of all values being equal to the base with
  // the empty string.
  if (max_delta == 0) {
    RTC_DCHECK(std::all_of(values.cbegin(), values.cend(),
                           [base](uint64_t val) { return val == base; }));
    return std::string();
  }

  const uint64_t delta_width_bits_unsigned = BitWidth(max_delta);
  const uint64_t delta_width_bits_signed =
      MaxSignedDeltaBitWidth(deltas, original_width_bits);

  // Note: Preference for unsigned if the two have the same width (efficiency).
  const bool signed_deltas =
      delta_width_bits_signed < delta_width_bits_unsigned;
  const uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  const bool deltas_optional = false;

  std::string encoding;
  FixedLengthDeltaEncoder encoder(original_width_bits, delta_width_bits,
                                  signed_deltas, deltas_optional);
  return encoder.Encode(base, values);
}

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(uint64_t original_width_bits,
                                                 uint64_t delta_width_bits,
                                                 bool signed_deltas,
                                                 bool deltas_optional)
    : original_width_bits_(original_width_bits),
      delta_width_bits_(delta_width_bits),
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
      absl::make_unique<BitWriter>(LowerBoundOutputLengthBytes(values.size()));

  EncodeHeader(writer.get());

  for (uint64_t value : values) {
    EncodeDelta(writer.get(), base, value);
    base = value;
  }

  return writer->GetString();
}

// TODO: !!! It might be that the deltas are even smaller than the values.
size_t FixedLengthDeltaEncoder::LowerBoundOutputLengthBytes(
    uint64_t num_of_deltas) const {
  const size_t length_bits = LowerBoundHeaderLengthBits() +
                             LowerBoundEncodedDeltasLengthBits(num_of_deltas);
  return BitsToBytes(length_bits);
}

size_t FixedLengthDeltaEncoder::LowerBoundHeaderLengthBits() const {
  return kBitsInHeaderForEncodingType + kBitsInHeaderForOriginalWidthBits +
         kBitsInHeaderForDeltaWidthBits + kBitsInHeaderForSignedDeltas +
         kBitsInHeaderForDeltasOptional;
}

size_t FixedLengthDeltaEncoder::LowerBoundEncodedDeltasLengthBits(
    uint64_t num_of_deltas) const {
  return num_of_deltas * (delta_width_bits_ + deltas_optional_);
}

void FixedLengthDeltaEncoder::EncodeHeader(BitWriter* writer) {
  RTC_DCHECK(writer);
  // Note: Since it's meaningless for a field to be of width 0, we encode
  // width == 1 as 0, width == 2 as 1, etc.
  // TODO: !!! Encoding type...?
  writer->WriteBits(
      static_cast<uint64_t>(EncodingType::kFixedSizeWithAllOptionalFields),
      kBitsInHeaderForEncodingType);
  writer->WriteBits(original_width_bits_ - 1,
                    kBitsInHeaderForOriginalWidthBits);
  writer->WriteBits(delta_width_bits_ - 1, kBitsInHeaderForDeltaWidthBits);
  writer->WriteBits(static_cast<uint64_t>(signed_deltas_),
                    kBitsInHeaderForSignedDeltas);
  writer->WriteBits(static_cast<uint64_t>(deltas_optional_),
                    kBitsInHeaderForDeltasOptional);
}

void FixedLengthDeltaEncoder::EncodeDelta(BitWriter* writer,
                                          uint64_t previous,
                                          uint64_t current) {
  RTC_DCHECK(writer);
  writer->WriteBits(ComputeDelta(previous, current, original_width_bits_),
                    delta_width_bits_);
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

  static bool ParseWithOnlyMandatoryFields(rtc::BitBuffer* reader,
                                           uint64_t* original_width_bits,
                                           uint64_t* delta_width_bits,
                                           bool* signed_deltas,
                                           bool* deltas_optional);

  static bool ParseWithAllOptionalFields(rtc::BitBuffer* reader,
                                         uint64_t* original_width_bits,
                                         uint64_t* delta_width_bits,
                                         bool* signed_deltas,
                                         bool* deltas_optional);

  FixedLengthDeltaDecoder(std::unique_ptr<rtc::BitBuffer> reader,
                          uint64_t original_width_bits,
                          uint64_t delta_width_bits,
                          bool signed_deltas,
                          bool deltas_optional,
                          uint64_t base,
                          size_t num_of_deltas);

  std::vector<uint64_t> Decode();

  bool GetDelta(uint64_t* delta);

  uint64_t ApplyDelta(uint64_t base, uint64_t delta) const;

  const std::unique_ptr<rtc::BitBuffer> reader_;

  const uint64_t original_width_bits_;

  const uint64_t delta_width_bits_;

  const bool signed_deltas_;
  const bool deltas_optional_;

  const uint64_t base_;
  const size_t num_of_deltas_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaDecoder);
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

  const auto encoding_type = static_cast<EncodingType>(encoding_type_bits);
  return encoding_type == EncodingType::kFixedSizeWithOnlyMandatoryFields ||
         encoding_type == EncodingType::kFixedSizeWithAllOptionalFields;
}

std::vector<uint64_t> FixedLengthDeltaDecoder::DecodeDeltas(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
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

  uint64_t original_width_bits;
  uint64_t delta_width_bits;
  bool signed_deltas;
  bool deltas_optional;

  bool encoding_type_parsed = false;
  switch (static_cast<EncodingType>(encoding_type_bits)) {
    case EncodingType::kFixedSizeWithOnlyMandatoryFields:
      if (!ParseWithOnlyMandatoryFields(reader.get(), &original_width_bits,
                                        &delta_width_bits, &signed_deltas,
                                        &deltas_optional)) {
        return nullptr;
      }
      encoding_type_parsed = true;
      break;
    case EncodingType::kFixedSizeWithAllOptionalFields:
      if (!ParseWithAllOptionalFields(reader.get(), &original_width_bits,
                                      &delta_width_bits, &signed_deltas,
                                      &deltas_optional)) {
        return nullptr;
      }
      encoding_type_parsed = true;
      break;
    case EncodingType::kReserved1:
    case EncodingType::kReserved2:
    case EncodingType::kNumberOfEncodingTypes:
      return nullptr;
  }
  if (!encoding_type_parsed) {
    RTC_LOG(LS_WARNING) << "Unrecognized encoding type.";
    return nullptr;
  }

  return absl::WrapUnique(new FixedLengthDeltaDecoder(
      std::move(reader), original_width_bits, delta_width_bits, signed_deltas,
      deltas_optional, base, num_of_deltas));
}

bool FixedLengthDeltaDecoder::ParseWithOnlyMandatoryFields(
    rtc::BitBuffer* reader,
    uint64_t* original_width_bits,
    uint64_t* delta_width_bits,
    bool* signed_deltas,
    bool* deltas_optional) {
  RTC_DCHECK(reader);
  RTC_DCHECK(original_width_bits);
  RTC_DCHECK(delta_width_bits);
  RTC_DCHECK(signed_deltas);
  RTC_DCHECK(deltas_optional);

  uint32_t read_buffer;
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltaWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);   // See encoding for -1's rationale.
  *delta_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  *original_width_bits = kDefaultOriginalWidthBits;
  *signed_deltas = kDefaultSignedDeltas;
  *deltas_optional = kDefaultDeltasOptional;

  return true;
}

bool FixedLengthDeltaDecoder::ParseWithAllOptionalFields(
    rtc::BitBuffer* reader,
    uint64_t* original_width_bits,
    uint64_t* delta_width_bits,
    bool* signed_deltas,
    bool* deltas_optional) {
  RTC_DCHECK(reader);
  RTC_DCHECK(original_width_bits);
  RTC_DCHECK(delta_width_bits);
  RTC_DCHECK(signed_deltas);
  RTC_DCHECK(deltas_optional);

  uint32_t read_buffer;

  // Original width
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForOriginalWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);      // See encoding for -1's rationale.
  *original_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  // Delta width
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltaWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);   // See encoding for -1's rationale.
  *delta_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  // Signed deltas
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForSignedDeltas)) {
    return false;
  }
  *signed_deltas = rtc::dchecked_cast<bool>(read_buffer);

  // Optional deltas
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltasOptional)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 1);
  *deltas_optional = rtc::dchecked_cast<bool>(read_buffer);

  return true;
}

FixedLengthDeltaDecoder::FixedLengthDeltaDecoder(
    std::unique_ptr<rtc::BitBuffer> reader,
    uint64_t original_width_bits,
    uint64_t delta_width_bits,
    bool signed_deltas,
    bool deltas_optional,
    uint64_t base,
    size_t num_of_deltas)
    : reader_(std::move(reader)),
      original_width_bits_(original_width_bits),
      delta_width_bits_(delta_width_bits),
      signed_deltas_(signed_deltas),
      deltas_optional_(deltas_optional),
      base_(base),
      num_of_deltas_(num_of_deltas) {
  RTC_DCHECK(reader_);
  // TODO(eladalon): Support signed deltas.
  RTC_DCHECK(!signed_deltas_) << "Not implemented.";
  // TODO(eladalon): Support optional deltas.
  RTC_DCHECK(!deltas_optional_) << "Not implemented.";
}

std::vector<uint64_t> FixedLengthDeltaDecoder::Decode() {
  std::vector<uint64_t> values(num_of_deltas_);

  uint64_t previous = base_;
  for (size_t i = 0; i < num_of_deltas_; ++i) {
    uint64_t delta;
    if (!GetDelta(&delta)) {
      return std::vector<uint64_t>();
    }
    values[i] = ApplyDelta(previous, delta);
    previous = values[i];
  }

  return values;
}

// TODO: !!! Invalidate on first error.
// TODO: !!! Optionals, signed, etc.
bool FixedLengthDeltaDecoder::GetDelta(uint64_t* delta) {
  RTC_DCHECK(reader_);

  // BitBuffer and BitBufferWriter read/write higher bits before lower bits.

  const size_t lower_bit_count = std::min<uint64_t>(delta_width_bits_, 32u);
  const size_t higher_bit_count =
      (delta_width_bits_ <= 32u) ? 0 : delta_width_bits_ - 32u;

  uint32_t lower_bits;
  uint32_t higher_bits;

  if (higher_bit_count > 0) {
    if (!reader_->ReadBits(&higher_bits, higher_bit_count)) {
      RTC_LOG(LS_WARNING) << "Failed to read higher half of delta.";
      return false;
    }
  } else {
    higher_bits = 0;
  }

  if (!reader_->ReadBits(&lower_bits, lower_bit_count)) {
    RTC_LOG(LS_WARNING) << "Failed to read lower half of delta.";
    return false;
  }

  const uint64_t lower_bits_64 = static_cast<uint64_t>(lower_bits);
  const uint64_t higher_bits_64 = static_cast<uint64_t>(higher_bits);

  *delta = (higher_bits_64 << 32) | lower_bits_64;
  return true;
}

uint64_t FixedLengthDeltaDecoder::ApplyDelta(uint64_t base,
                                             uint64_t delta) const {
  RTC_DCHECK_LE(base, MaxValueOfBitWidth(original_width_bits_));
  RTC_DCHECK_LE(delta, MaxValueOfBitWidth(delta_width_bits_));

  RTC_DCHECK(!signed_deltas_) << "Not implemented.";
  RTC_DCHECK(!deltas_optional_) << "Not implemented.";

  // TODO: !!! Do I really need this comment? What was I concerned about?
  // * If |original_width_bits_| == 64, wrapping-around when adding |base|
  //   and |delta| is the right behavior.
  // * If |original_width_bits_| < 64, so is |delta_width_bits_|, and so
  //   adding base and delta is guaranteed not to wrap around the uint64_t.
  RTC_DCHECK_LE(delta_width_bits_, original_width_bits_);  // Reminder.
  uint64_t result = base + delta;
  if (original_width_bits_ < 64) {
    result %= (1 << original_width_bits_);
  }
  return result;
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
  RTC_DCHECK_GT(num_of_deltas, 0);  // Allows empty vector to indicate error.

  // The empty string is a special case indicating that all values were equal
  // to the base.
  if (input.empty()) {
    std::vector<uint64_t> result(num_of_deltas);
    std::fill(result.begin(), result.end(), base);
    return result;
  }

  if (FixedLengthDeltaDecoder::IsSuitableDecoderFor(input)) {
    return FixedLengthDeltaDecoder::DecodeDeltas(input, base, num_of_deltas);
  }

  RTC_LOG(LS_WARNING) << "Could not decode delta-encoded stream.";
  return std::vector<uint64_t>();
}

}  // namespace webrtc
