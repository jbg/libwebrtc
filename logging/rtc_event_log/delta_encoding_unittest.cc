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

#include <string>
#include <vector>

#include "rtc_base/bitbuffer.h"  // TODO: !!!
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(DeltaEncodingTest, E1) {
  std::string buffer(100, '\0');

  rtc::BitBufferWriter writer(reinterpret_cast<uint8_t*>(&buffer[0]),
                              buffer.size());
  rtc::BitBuffer reader(reinterpret_cast<uint8_t*>(&buffer[0]), buffer.size());

  ASSERT_TRUE(writer.WriteBits(0x567890abcdef, 48));
  uint32_t a, b;
  ASSERT_TRUE(reader.ReadBits(&a, 32));
  ASSERT_TRUE(reader.ReadBits(&b, 16));
  uint64_t aa = a, bb = b;
  uint64_t ab = (aa << 16) | bb;
  printf("a = |%x|\n", a);
  printf("b = |%x|\n", b);
  printf("ab = |%lx|\n", ab);
  // TODO: !!!
}

// TODO: !!! Replace by "all equal to base returns empty string".
TEST(DeltaEncodingTest, SingleElementEqualToBase) {
  const uint64_t base = 3432;  // TODO: !!!
  const std::vector<uint64_t> values = {base};

  printf("--- %d\n", __LINE__);

  const std::string encoded = EncodeDeltas(base, values);
  printf("--- %d, encoded = %lu, [%s]\n", __LINE__, encoded.length(), encoded.c_str());
  ASSERT_FALSE(encoded.empty());

  printf("--- %d\n", __LINE__);
  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());
  printf("--- %d\n", __LINE__);

  EXPECT_EQ(decoded, values);
}

TEST(DeltaEncodingTest, SingleElementDifferentFromBase) {}

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
