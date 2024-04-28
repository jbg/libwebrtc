/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/audio_view.h"

#include "test/gtest.h"

namespace webrtc {

namespace {}  // namespace

TEST(AudioViewTest, Basic) {
  const size_t kArraySize = 100u;
  int16_t arr[kArraySize] = {};

  MonoView<int16_t> mono(arr);
  MonoView<const int16_t> const_mono(arr);
  EXPECT_EQ(mono.size(), kArraySize);
  EXPECT_EQ(const_mono.size(), kArraySize);
  EXPECT_EQ(&mono[0], &const_mono[0]);

  EXPECT_EQ(1u, NumChannels(mono));
  EXPECT_EQ(1u, NumChannels(const_mono));
  EXPECT_EQ(100u, SamplesPerChannel(mono));
  EXPECT_TRUE(IsMono(mono));
  EXPECT_TRUE(IsMono(const_mono));

  InterleavedView<int16_t> interleaved(arr, 2, 50);
  InterleavedView<const int16_t> const_interleaved(arr, 2, 50);
  EXPECT_EQ(NumChannels(interleaved), 2u);
  EXPECT_TRUE(!IsMono(interleaved));
  EXPECT_TRUE(!IsMono(const_interleaved));
  EXPECT_EQ(NumChannels(const_interleaved), 2u);
  EXPECT_EQ(SamplesPerChannel(interleaved), 50u);

  interleaved = InterleavedView<int16_t>(arr, 4);
  EXPECT_EQ(NumChannels(interleaved), 4u);
  InterleavedView<const int16_t> const_interleaved2(interleaved);
  EXPECT_EQ(NumChannels(const_interleaved2), 4u);
  EXPECT_EQ(SamplesPerChannel(interleaved), 25u);

  DeinterleavedView<int16_t> di(arr, 10, 10);
  EXPECT_EQ(NumChannels(di), 10u);
  EXPECT_EQ(SamplesPerChannel(di), 10u);
  EXPECT_TRUE(!IsMono(di));
  auto mono_ch = di.AsMono();
  EXPECT_EQ(NumChannels(mono_ch), 1u);
  EXPECT_EQ(SamplesPerChannel(mono_ch), 10u);
}

}  // namespace webrtc
