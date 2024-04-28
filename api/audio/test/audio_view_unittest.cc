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

TEST(AudioViewTest, FrameStartsZeroedAndMuted) {
  int16_t arr[100] = {};
  MonoView<int16_t> mono(arr);
  MonoView<const int16_t> const_mono(arr);
  RTC_DCHECK_EQ(1u, NumChannels(mono));
  RTC_DCHECK_EQ(1u, NumChannels(const_mono));
  RTC_DCHECK_EQ(100u, SamplesPerChannel(mono));
  RTC_DCHECK(IsMono(mono));
  RTC_DCHECK(IsMono(const_mono));

  InterleavedView<int16_t> interleaved(arr, 2, 50);
  InterleavedView<const int16_t> const_interleaved(arr, 2, 50);
  RTC_DCHECK_EQ(NumChannels(interleaved), 2);
  RTC_DCHECK(!IsMono(interleaved));
  RTC_DCHECK(!IsMono(const_interleaved));
  RTC_DCHECK_EQ(NumChannels(const_interleaved), 2);
  RTC_DCHECK_EQ(SamplesPerChannel(interleaved), 50u);

  interleaved = InterleavedView<int16_t>(arr, 4);
  RTC_DCHECK_EQ(NumChannels(interleaved), 4);
  InterleavedView<const int16_t> const_interleaved2(interleaved);
  RTC_DCHECK_EQ(NumChannels(const_interleaved2), 4);
  RTC_DCHECK_EQ(SamplesPerChannel(interleaved), 25u);

  DeinterleavedView<int16_t> di(arr, 10, 10);
  RTC_DCHECK_EQ(NumChannels(di), 10u);
  RTC_DCHECK_EQ(SamplesPerChannel(di), 10u);
  RTC_DCHECK(!IsMono(di));
  auto mono_ch = di.AsMono();
  RTC_DCHECK_EQ(NumChannels(mono_ch), 1u);
  RTC_DCHECK_EQ(SamplesPerChannel(mono_ch), 10u);
}

}  // namespace webrtc
