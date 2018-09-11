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

#include <memory>  // TODO: !!! unique_ptr necessary?

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

class FixedLengthDeltaEncoder final : public DeltaEncoder {
 public:
  FixedLengthDeltaEncoder(uint8_t delta_width_bits,
                          bool signed_deltas,
                          uint8_t original_width_bit);
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

  bool EncodeHeader();

  bool EncodeDelta(uint64_t previous, uint64_t current);

  void AbortEncoding(std::string* output);

  // TODO: !!!
  const uint8_t delta_width_bits_;

  // TODO: !!!
  const bool signed_deltas_ __attribute__ ((unused));  // TODO: !!!

  // TODO: !!!
  const uint8_t original_width_bit_;

  // TODO: !!!
  const bool deltas_optional_;

  // TODO: !!!
  std::unique_ptr<rtc::BitBufferWriter> buffer_;
};

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(uint8_t delta_width_bits,
                                                 bool signed_deltas,
                                                 uint8_t original_width_bit)
    : delta_width_bits_(delta_width_bits),
      signed_deltas_(signed_deltas),
      original_width_bit_(original_width_bit),
      deltas_optional_(false) {
  RTC_DCHECK_LT(delta_width_bits_, 1 << 6);
  RTC_DCHECK_LT(original_width_bit_, 1 << 6);
}

bool FixedLengthDeltaEncoder::Encode(uint64_t base,
                                     std::vector<uint64_t> values,
                                     std::string* output) {
  RTC_DCHECK(output);
  RTC_DCHECK(output->empty());
  RTC_DCHECK(!values.empty());

  AllocateOutput(values.size(), output);

  if (!EncodeHeader()) {
    AbortEncoding(output);
    return false;
  }
  
  for (uint64_t value : values) {
    if (!EncodeDelta(base, value)) {
      AbortEncoding(output);
      return false;
    }
    base = value;
  }

  return true;
}

size_t FixedLengthDeltaEncoder::EstimateOutputLengthBytes(
    size_t num_of_deltas) const {
  const size_t length_bits =
      EstimateHeaderLengthBits() + EstimateEncodedDeltasLengthBits(num_of_deltas);
  return (length_bits / 8) + (length_bits % 8 > 0 ? 1 : 0);
}

size_t FixedLengthDeltaEncoder::EstimateHeaderLengthBits() const {
  return 10;  // TODO: !!!
}

size_t FixedLengthDeltaEncoder::EstimateEncodedDeltasLengthBits(
    size_t num_of_deltas) const {
  return num_of_deltas * (delta_width_bits_ + deltas_optional_);
}

void FixedLengthDeltaEncoder::AllocateOutput(size_t num_of_deltas,
                                             std::string* output) {
  RTC_DCHECK(output);
  RTC_DCHECK(output->empty());
  output->resize(EstimateOutputLengthBytes(num_of_deltas  ));
  uint8_t* fix_me = reinterpret_cast<uint8_t*>(&output[0]); // TODO: !!!
  buffer_ = absl::make_unique<rtc::BitBufferWriter>(fix_me, output->size());
}

bool FixedLengthDeltaEncoder::EncodeHeader() {
  // TODO: !!!

  return true;  // TODO: !!!
}

bool FixedLengthDeltaEncoder::EncodeDelta(uint64_t previous, uint64_t current) {

  return true;  // TODO: !!!
}

void FixedLengthDeltaEncoder::AbortEncoding(std::string* output) {
  buffer_.reset();
  output->clear();  
}

}  // namespace
}  // namespace webrtc
