/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/drift_detector.h"

#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gtest.h"

namespace webrtc {

TEST(DriftDetector, UndefinedInitialDetection) {
  DriftDetector detector(100);
  for (size_t k = 0; k < 10; ++k) {
    EXPECT_FALSE(detector.Update(k, 1.f));
  }
}

TEST(DriftDetector, NoDriftDetection) {
  DriftDetector detector(100);
  for (size_t k = 0; k < 30; ++k) {
    detector.Update(k, 1.f);
  }
  EXPECT_FALSE(detector.Update(31.f, 1.f));
}

TEST(DriftDetector, DriftDetection) {
  DriftDetector detector(100);
  for (size_t k = 0; k < 30; ++k) {
    detector.Update(k, k);
  }
  EXPECT_TRUE(detector.Update(31.f, 31.f));
  EXPECT_EQ(1.f, *detector.Update(32.f, 32.f));
}

TEST(DriftDetector, ValidDriftCheck) {
  DriftDetector detector(100);
  for (size_t k = 0; k < 100; ++k) {
    EXPECT_FALSE(detector.Update(k % 30, k % 2));
  }
}

TEST(DriftDetector, NonStickyDrift) {
  DriftDetector detector(100);
  for (size_t k = 0; k < 30; ++k) {
    detector.Update(k, k);
  }

  for (size_t k = 30; k < 130; ++k) {
    detector.Update(k, 1);
  }
  EXPECT_FALSE(detector.Update(130.f, 1.f));
}

}  // namespace webrtc
