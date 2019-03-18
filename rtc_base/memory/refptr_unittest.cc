/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/refptr.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
struct PodStruct {
  PodStruct(int f1, int f2) : field1(f1), field2(f2) {}
  int field1;
  int field2;
};

}  // namespace
TEST(RefPtrTest, EmptyRefIsEmpty) {
  RefPtr<PodStruct> pod;
  EXPECT_FALSE(pod);
}

TEST(RefPtrTest, FillediIsFilled) {
  RefPtr<PodStruct> pod(PodStruct{1, 2});
  EXPECT_TRUE(pod);
  EXPECT_EQ(pod->field2, 2);
}
TEST(RefPtrTest, MakesStruct) {
  auto pod = MakeRefPtr<PodStruct>(1, 2);
  EXPECT_EQ(pod->field2, 2);
}

TEST(RefPtrTest, WrapsStruct) {
  auto pod = WrapRefPtr(PodStruct{1, 2});
  EXPECT_EQ(pod->field2, 2);
}
}  // namespace webrtc
