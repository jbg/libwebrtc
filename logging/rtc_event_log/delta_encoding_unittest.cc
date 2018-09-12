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
#include <string>
#include <vector>

#include "rtc_base/bitbuffer.h"  // TODO: !!!
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// TODO: !!!
class DeltaEncodingTest : public ::testing::TestWithParam<bool> {
 public:
  DeltaEncodingTest() {}
  ~DeltaEncodingTest() override = default;

  // TODO: !!!
  void TestEncodingAndDecoding(uint64_t base,
                               const std::vector<uint64_t>& values) {
    const std::string encoded = EncodeDeltas(base, values);
    ASSERT_FALSE(encoded.empty());

    const std::vector<uint64_t> decoded =
        DecodeDeltas(encoded, base, values.size());

    EXPECT_EQ(decoded, values);
  }
};

// TODO: !!! Replace by "all equal to base returns empty string".
TEST_F(DeltaEncodingTest, SingleElementEqualToBase) {
  const uint64_t base = 3432;
  const std::vector<uint64_t> values = {base};
  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MinDeltaNoWrapAround) {
  const uint64_t base = 3432;

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {base + 1};
  } else {
    values = {base + 1, base + 2};
  }
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaNoWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = 3432;

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {base + kBigDelta};
  } else {
    values = {base + kBigDelta, base + kBigDelta + 1};
  }
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaNoWrapAround) {
  const uint64_t base = 3432;

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {std::numeric_limits<uint64_t>::max()};
  } else {
    values = {std::numeric_limits<uint64_t>::max() - 1,
              std::numeric_limits<uint64_t>::max()};
  }
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MinDeltaWithWrapAround) {
  const uint64_t base = std::numeric_limits<uint64_t>::max();

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {0};
  } else {
    values = {10, 20};
  }
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaWithWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = std::numeric_limits<uint64_t>::max() - kBigDelta + 3;

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {base + kBigDelta};
  } else {
    values = {base + kBigDelta, base + kBigDelta + 1};
  }
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaWithWrapAround) {
  const uint64_t base = 3432;

  std::vector<uint64_t> values;
  if (GetParam()) {
    values = {base + std::numeric_limits<uint64_t>::max()};
  } else {
    values = {base + 12, base + std::numeric_limits<uint64_t>::max()};
  }
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_CASE_P(SingleElement, DeltaEncodingTest, ::testing::Bool());

#if 0
TODO: !!! Test these cases:

Empty list - rejected? Crashes?
Single element in list.
  Single element is the same as the base.
  Single element is different from the base.
100 elements, no wrap-around.
100 elements, no wrap-around.

#endif

}  // namespace
}  // namespace webrtc
