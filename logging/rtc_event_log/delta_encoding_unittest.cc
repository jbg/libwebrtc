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

// TODO: !!! Replace by "all equal to base returns empty string".
TEST(DeltaEncodingTest, SingleElementEqualToBase) {
  const uint64_t base = 3432;  // TODO: !!!
  const std::vector<uint64_t> values = {base};

  const std::string encoded = EncodeDeltas(base, values);
  ASSERT_FALSE(encoded.empty());

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

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
