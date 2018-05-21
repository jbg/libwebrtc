/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/create_video_quality_test_fixture.h"
#include "video/video_quality_test.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

std::unique_ptr<VideoQualityTestFixtureInterface>
CreateVideoQualityTestFixture() {
  // By default, we don't override the FEC module, so pass an empty factory.
  // TODO(phoglund): Pass a default factory that creates the default factory
  // and clean up null checks on factory in video_quality_test.cc.
  return rtc::MakeUnique<VideoQualityTest>(nullptr);
}

}  // namespace webrtc


