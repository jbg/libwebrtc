/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/video_quality_test_fixture.h"

namespace webrtc {

VideoQualityTestFixtureInterface::Params::Video::Video() = default;
VideoQualityTestFixtureInterface::Params::Video::~Video() = default;

VideoQualityTestFixtureInterface::Params::Screenshare::Screenshare() = default;
VideoQualityTestFixtureInterface::Params::Screenshare::~Screenshare() = default;

VideoQualityTestFixtureInterface::Params::Analyzer::Analyzer() = default;
VideoQualityTestFixtureInterface::Params::Analyzer::~Analyzer() = default;

VideoQualityTestFixtureInterface::Params::SS::SS() = default;
VideoQualityTestFixtureInterface::Params::SS::~SS() = default;

VideoQualityTestFixtureInterface::Params::Logging::Logging() = default;
VideoQualityTestFixtureInterface::Params::Logging::~Logging() = default;

}  // namespace webrtc
