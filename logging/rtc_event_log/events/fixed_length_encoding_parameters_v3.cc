/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/fixed_length_encoding_parameters_v3.h"

#include <algorithm>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/logging.h"

namespace webrtc {

FixedLengthEncodingParametersV3
FixedLengthEncodingParametersV3::CalculateParameters(
    uint64_t base,
    const rtc::ArrayView<const uint64_t> values,
    uint64_t value_width_bits,
    bool values_optional) {
  // As a special case, if all of the elements are identical to the base
  // we just encode the base value with a special delta header.
  if (std::all_of(values.cbegin(), values.cend(),
                  [base](uint64_t val) { return val == base; })) {
    // Delta header with signed=true and delta_bitwidth=64
    return FixedLengthEncodingParametersV3(/*delta_width_bits=*/64,
                                           /*signed_deltas=*/true,
                                           values_optional, value_width_bits);
  }

  const uint64_t bit_mask =
      webrtc_event_logging::MaxUnsignedValueOfBitWidth(value_width_bits);

  // Calculate the bitwidth required to encode all deltas when using a
  // unsigned or signed represenation, respectively. For the unsigned
  // representation, we just track the largest delta. For the signed
  // representation, we have two possibilities for each delta; either
  // going "forward" (i.e. current - previous) or "backwards"
  // (i.e. previous - current) where both values are calculated with
  // wrap around. We then track the largest positive and negative
  // magnitude across the batch, assuming that we choose the smaller
  // delta for each element.
  uint64_t max_unsigned_delta = 0;
  uint64_t max_pos_signed_delta = 0;
  uint64_t min_neg_signed_delta = 0;
  uint64_t prev = base;
  for (const uint64_t current : values) {
    const uint64_t forward_delta =
        webrtc_event_logging::UnsignedDelta(prev, current, bit_mask);
    const uint64_t backward_delta =
        webrtc_event_logging::UnsignedDelta(current, prev, bit_mask);

    max_unsigned_delta = std::max(max_unsigned_delta, forward_delta);

    if (forward_delta < backward_delta) {
      max_pos_signed_delta = std::max(max_pos_signed_delta, forward_delta);
    } else {
      min_neg_signed_delta = std::max(min_neg_signed_delta, backward_delta);
    }

    prev = current;
  }

  // We now know the largest unsigned delta and the largest magnitudes of
  // positive and negative signed deltas. Get the bitwidths required for
  // each of the two encodings.
  const uint64_t delta_width_bits_unsigned =
      webrtc_event_logging::UnsignedBitWidth(max_unsigned_delta);
  const uint64_t delta_width_bits_signed = webrtc_event_logging::SignedBitWidth(
      max_pos_signed_delta, min_neg_signed_delta);

  // Note: Preference for unsigned if the two have the same width (efficiency).
  bool signed_deltas = delta_width_bits_signed < delta_width_bits_unsigned;
  uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  // signed_deltas && delta_width_bits==64 is reserved for "all values equal".
  RTC_DCHECK(!signed_deltas || delta_width_bits < 64);

  RTC_DCHECK(ValidParameters(delta_width_bits, signed_deltas, values_optional,
                             value_width_bits));
  return FixedLengthEncodingParametersV3(delta_width_bits, signed_deltas,
                                         values_optional, value_width_bits);
}

uint64_t FixedLengthEncodingParametersV3::DeltaHeaderAsInt() const {
  uint64_t header = delta_width_bits_ - 1;
  RTC_CHECK_LT(header, 1u << 6);
  if (signed_deltas_) {
    header += 1u << 6;
  }
  RTC_CHECK_LT(header, 1u << 7);
  if (values_optional_) {
    header += 1u << 7;
  }
  return header;
}

absl::optional<FixedLengthEncodingParametersV3>
FixedLengthEncodingParametersV3::ParseDeltaHeader(uint64_t header,
                                                  uint64_t value_width_bits) {
  uint64_t delta_width_bits = (header & ((1u << 6) - 1)) + 1;
  bool signed_deltas = header & (1u << 6);
  bool values_optional = header & (1u << 7);

  if (header >= (1u << 8)) {
    RTC_LOG(LS_ERROR) << "Failed to parse delta header; unread bits remaining.";
    return absl::nullopt;
  }

  if (!ValidParameters(delta_width_bits, signed_deltas, values_optional,
                       value_width_bits)) {
    RTC_LOG(LS_ERROR) << "Failed to parse delta header. Invalid combination of "
                         "values: delta_width_bits="
                      << delta_width_bits << " signed_deltas=" << signed_deltas
                      << " values_optional=" << values_optional
                      << " value_width_bits=" << value_width_bits;
    return absl::nullopt;
  }

  return FixedLengthEncodingParametersV3(delta_width_bits, signed_deltas,
                                         values_optional, value_width_bits);
}

}  // namespace webrtc
