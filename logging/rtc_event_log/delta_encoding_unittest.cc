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
#include <numeric>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Encodes |values| based on |base|, then decodes the result and makes sure
// that it is equal to the original input.
// If |encoded_string| is non-null, the encoded result will also be written
// into it.
void TestEncodingAndDecoding(uint64_t base,
                             const std::vector<uint64_t>& values,
                             std::string* encoded_string = nullptr) {
  const std::string encoded = EncodeDeltas(base, values);
  if (encoded_string) {
    *encoded_string = encoded;
  }

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

std::vector<uint64_t> CreateSequenceByFirstValue(uint64_t first,
                                                 size_t sequence_length) {
  std::vector<uint64_t> sequence(sequence_length);
  std::iota(sequence.begin(), sequence.end(), first);
  return sequence;
}

std::vector<uint64_t> CreateSequenceByLastValue(uint64_t last,
                                                size_t num_elements) {
  const uint64_t first = last - num_elements + 1;
  std::vector<uint64_t> result(num_elements);
  std::iota(result.begin(), result.end(), first);
  return result;
}

// If |sequence_length| is greater than the number of deltas, the sequence of
// deltas will wrap around.
std::vector<uint64_t> CreateSequenceByDeltas(
    uint64_t first,
    const std::vector<uint64_t>& deltas,
    size_t sequence_length) {
  std::vector<uint64_t> sequence(sequence_length);

  RTC_DCHECK_GE(sequence_length, 1);
  sequence[0] = first;
  for (size_t i = 1, next_delta_index = 0; i < sequence.size(); ++i) {
    sequence[i] = sequence[i - 1] + deltas[next_delta_index];
    next_delta_index = (next_delta_index + 1) % deltas.size();
  }

  return sequence;
}

// Tests of the delta encoding, parameterized by the number of elements
// in the sequence created by the test.
class DeltaEncodingTest : public ::testing::TestWithParam<size_t> {
 public:
  DeltaEncodingTest() {}
  ~DeltaEncodingTest() override = default;
};

TEST_P(DeltaEncodingTest, AllElementsEqualToBaseElement) {
  const uint64_t base = 3432;
  std::vector<uint64_t> values(GetParam());
  std::fill(values.begin(), values.end(), base);
  std::string encoded;
  TestEncodingAndDecoding(base, values, &encoded);

  // Additional requirement - the encoding should be efficient in this
  // case - the empty string will be used.
  EXPECT_TRUE(encoded.empty());
}

TEST_P(DeltaEncodingTest, MinDeltaNoWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByFirstValue(base + 1, GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaNoWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = 3432;

  const auto values = CreateSequenceByFirstValue(base + kBigDelta, GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaNoWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByLastValue(
      std::numeric_limits<uint64_t>::max(), GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MinDeltaWithWrapAround) {
  const uint64_t base = std::numeric_limits<uint64_t>::max();

  const auto values = CreateSequenceByDeltas(0, {10, 3}, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

// Suppress "integral constant overflow" warning; this is the test's focus.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4307)
#endif
TEST_P(DeltaEncodingTest, BigDeltaWithWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = std::numeric_limits<uint64_t>::max() - kBigDelta + 3;

  const auto values = CreateSequenceByFirstValue(base + kBigDelta, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

TEST_P(DeltaEncodingTest, MaxDeltaWithWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByLastValue(3, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_CASE_P(NumberOfElementsInSequence,
                        DeltaEncodingTest,
                        ::testing::Values(1, 2, 100, 10000));

}  // namespace
}  // namespace webrtc
