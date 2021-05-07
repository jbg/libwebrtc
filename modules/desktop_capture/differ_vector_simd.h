/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This header file is used only differ_block.h. It defines the SIMD rountines
// for finding vector difference.

#ifndef MODULES_DESKTOP_CAPTURE_DIFFER_VECTOR_SIMD_H_
#define MODULES_DESKTOP_CAPTURE_DIFFER_VECTOR_SIMD_H_

#include <stdint.h>

namespace webrtc {

// Find vector difference of dimension 16.
extern bool VectorDifference_SIMD_W16(const uint8_t* image1,
                                      const uint8_t* image2);

// Find vector difference of dimension 32.
extern bool VectorDifference_SIMD_W32(const uint8_t* image1,
                                      const uint8_t* image2);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_DIFFER_VECTOR_SIMD_H_
