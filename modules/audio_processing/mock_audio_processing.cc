/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/mock_audio_processing.h"

namespace webrtc {

namespace test {

MockEchoCancellation::MockEchoCancellation() = default;
MockEchoCancellation::~MockEchoCancellation() = default;

MockEchoControlMobile::MockEchoControlMobile() = default;
MockEchoControlMobile::~MockEchoControlMobile() = default;

MockGainControl::MockGainControl() = default;
MockGainControl::~MockGainControl() = default;

MockHighPassFilter::MockHighPassFilter() = default;
MockHighPassFilter::~MockHighPassFilter() = default;

MockLevelEstimator::MockLevelEstimator() = default;
MockLevelEstimator::~MockLevelEstimator() = default;

MockNoiseSuppression::MockNoiseSuppression() = default;
MockNoiseSuppression::~MockNoiseSuppression() = default;

MockCustomProcessing::MockCustomProcessing() = default;
MockCustomProcessing::~MockCustomProcessing() = default;

MockEchoControl::MockEchoControl() = default;
MockEchoControl::~MockEchoControl() = default;

MockVoiceDetection::MockVoiceDetection() = default;
MockVoiceDetection::~MockVoiceDetection() = default;

MockAudioProcessing::MockAudioProcessing()
    : echo_cancellation_(new testing::NiceMock<MockEchoCancellation>()),
      echo_control_mobile_(new testing::NiceMock<MockEchoControlMobile>()),
      gain_control_(new testing::NiceMock<MockGainControl>()),
      high_pass_filter_(new testing::NiceMock<MockHighPassFilter>()),
      level_estimator_(new testing::NiceMock<MockLevelEstimator>()),
      noise_suppression_(new testing::NiceMock<MockNoiseSuppression>()),
      voice_detection_(new testing::NiceMock<MockVoiceDetection>()) {}
MockAudioProcessing::~MockAudioProcessing() = default;

}  // namespace test
}  // namespace webrtc
