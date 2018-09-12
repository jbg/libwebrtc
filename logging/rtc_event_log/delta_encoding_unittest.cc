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
class DeltaEncodingTest : public ::testing::Test {
 public:
  DeltaEncodingTest() {}
  ~DeltaEncodingTest() override = default;
};

// TODO: !!! Replace by "all equal to base returns empty string".
TEST_F(DeltaEncodingTest, SingleElementEqualToBase) {
  const uint64_t base = 3432;
  const std::vector<uint64_t> values = {base};

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest, SingleElementDifferentFromBaseMinDeltaNoWrapAround) {
  const uint64_t base = 3432;
  const std::vector<uint64_t> values = {base + 1};
  ASSERT_GT(values[0], base) << "Test sanity; must not wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest, SingleElementDifferentFromBaseBigDeltaNoWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = 3432;
  const std::vector<uint64_t> values = {base + kBigDelta};
  ASSERT_GT(values[0], base) << "Test sanity; must not wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest, SingleElementDifferentFromBaseMaxDeltaNoWrapAround) {
  const uint64_t base = 3432;
  const std::vector<uint64_t> values = {std::numeric_limits<uint64_t>::max()};
  ASSERT_GT(values[0], base) << "Test sanity; must not wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest,
       SingleElementDifferentFromBaseMinDeltaWithWrapAround) {
  const uint64_t base = std::numeric_limits<uint64_t>::max();
  const std::vector<uint64_t> values = {base + 1};
  ASSERT_LT(values[0], base) << "Test sanity; must wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest,
       SingleElementDifferentFromBaseBigDeltaWithWrapAround) {
  const uint64_t kBigDelta = 132828;

  const uint64_t base = std::numeric_limits<uint64_t>::max() - kBigDelta + 3;
  const std::vector<uint64_t> values = {base + kBigDelta};
  ASSERT_LT(values[0], base) << "Test sanity; must wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

TEST_F(DeltaEncodingTest,
       SingleElementDifferentFromBaseMaxDeltaWithWrapAround) {
  const uint64_t base = 3432;  // TODO: !!!
  const std::vector<uint64_t> values = {base +
                                        std::numeric_limits<uint64_t>::max()};
  ASSERT_LT(values[0], base) << "Test sanity; must wrap around";

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

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
