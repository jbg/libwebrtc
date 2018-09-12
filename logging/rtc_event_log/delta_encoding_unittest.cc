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

#include "rtc_base/bitbuffer.h"  // TODO: !!!
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ELAD, E1) {
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

}  // namespace
}  // namespace webrtc
