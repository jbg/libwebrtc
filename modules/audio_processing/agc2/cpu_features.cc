/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/cpu_features.h"

#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {

std::string AvailableCpuFeatures::ToString() const {
  char buf[64];
  rtc::SimpleStringBuilder builder(buf);
  bool first = true;
  if (sse2) {
    builder << (first ? "SSE2" : "_SSE2");
    first = false;
  }
  if (avx2) {
    builder << (first ? "AVX2" : "_AVX2");
    first = false;
  }
  if (neon) {
    builder << (first ? "NEON" : "_NEON");
    first = false;
  }
  if (first) {
    return "none";
  }
  return builder.str();
}

// Detects available CPU features.
AvailableCpuFeatures GetAvailableCpuFeatures() {
  return {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    /*sse2=*/GetCPUInfo(kSSE2) != 0,
        /*avx2=*/GetCPUInfo(kAVX2) != 0,
#else
    /*sse2=*/false,
        /*avx2=*/false,
#endif
#if defined(WEBRTC_HAS_NEON)
        /*neon=*/true
#else
        /*neon=*/false
#endif
  };
}

}  // namespace webrtc
