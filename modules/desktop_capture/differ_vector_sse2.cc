/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/differ_vector_sse2.h"

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <emmintrin.h>
#include <mmintrin.h>
#endif

namespace webrtc {

namespace {
extern bool VectorDifference_SSE2(const uint8_t* image1,
                                  const uint8_t* image2,
                                  const int block_size) {
  __m128i v0;
  __m128i v1;
  const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
  const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

  const int vector_limit = block_size >> 2;
  for (int i = 0; i < vector_limit; ++i) {
    v0 = _mm_loadu_si128(i1 + i);
    v1 = _mm_loadu_si128(i2 + i);
    __m128i cmp = _mm_cmpeq_epi8(v0, v1);
    unsigned bitmask = _mm_movemask_epi8(cmp);
    if (bitmask != 0xFFFFU)
      return true;
  }
  return false;
}
}  // namespace

extern bool VectorDifference_SSE2_W16(const uint8_t* image1,
                                      const uint8_t* image2) {
  return VectorDifference_SSE2(image1, image2, 16);
}

extern bool VectorDifference_SSE2_W32(const uint8_t* image1,
                                      const uint8_t* image2) {
  return VectorDifference_SSE2(image1, image2, 32);
}

}  // namespace webrtc
