/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/include/audio_util.h"

namespace webrtc {

void MyFunc() {
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

void FloatToS16(const float* src, size_t size, int16_t* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = FloatToS16(src[i]);
}

void S16ToFloat(const int16_t* src, size_t size, float* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = S16ToFloat(src[i]);
}

void S16ToFloatS16(const int16_t* src, size_t size, float* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = src[i];
}

void FloatS16ToS16(const float* src, size_t size, int16_t* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = FloatS16ToS16(src[i]);
}

void FloatToFloatS16(const float* src, size_t size, float* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = FloatToFloatS16(src[i]);
}

void FloatS16ToFloat(const float* src, size_t size, float* dest) {
  for (size_t i = 0; i < size; ++i)
    dest[i] = FloatS16ToFloat(src[i]);
}

template <>
void DownmixInterleavedToMono<int16_t>(const int16_t* interleaved,
                                       size_t num_frames,
                                       int num_channels,
                                       int16_t* deinterleaved) {
  DownmixInterleavedToMonoImpl<int16_t, int32_t>(interleaved, num_frames,
                                                 num_channels, deinterleaved);
}

}  // namespace webrtc
