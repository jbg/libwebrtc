/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <xmmintrin.h>

#include "common_audio/resampler/sinc_resampler.h"

namespace webrtc {

float SincResampler::Convolve_AVX2(const float* input_ptr,
                                   const float* k1,
                                   const float* k2,
                                   double kernel_interpolation_factor) {
  __m256 m_input;
  __m256 m_sums1 = _mm256_setzero_ps();
  __m256 m_sums2 = _mm256_setzero_ps();

  // Based on |input_ptr| alignment, we need to use loadu or load.  Unrolling
  // these loops has not been tested or benchmarked.
  bool aligned_input = (reinterpret_cast<uintptr_t>(input_ptr) & 0x1F) == 0;
  if (!aligned_input) {
    for (size_t i = 0; i < kKernelSize; i += 8) {
      m_input = _mm256_loadu_ps(input_ptr + i);
      m_sums1 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k1 + i), m_sums1);
      m_sums2 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k2 + i), m_sums2);
    }
  } else {
    for (size_t i = 0; i < kKernelSize; i += 8) {
      m_input = _mm256_load_ps(input_ptr + i);
      m_sums1 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k1 + i), m_sums1);
      m_sums2 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k2 + i), m_sums2);
    }
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = _mm256_fmadd_ps(
      m_sums1,
      _mm256_set1_ps(static_cast<float>(1.0 - kernel_interpolation_factor)),
      _mm256_mul_ps(m_sums2, _mm256_set1_ps(static_cast<float>(
                                 kernel_interpolation_factor))));

  // Sum components together.
  float* v = reinterpret_cast<float*>(&m_sums1);
  float result = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
  return result;
}

}  // namespace webrtc
