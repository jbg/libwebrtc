/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "components/video_codec/RTCH264ProfileLevelId.h"

#import <XCTest/XCTest.h>

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "sdk/objc/unittests/xctest_to_gtest.h"
#endif

@interface RTCH264ProfileLevelIdTests : XCTestCase

@end

static NSString *level31ConstrainedHigh = @"640c1f";
static NSString *level31ConstrainedBaseline = @"42e01f";

@implementation RTCH264ProfileLevelIdTests

- (void)testInitWithString {
  RTC_OBJC_TYPE(RTCH264ProfileLevelId) *profileLevelId =
      [[RTC_OBJC_TYPE(RTCH264ProfileLevelId) alloc] initWithHexString:level31ConstrainedHigh];
  XCTAssertEqual(profileLevelId.profile, RTCH264ProfileConstrainedHigh);
  XCTAssertEqual(profileLevelId.level, RTCH264Level3_1);

  profileLevelId =
      [[RTC_OBJC_TYPE(RTCH264ProfileLevelId) alloc] initWithHexString:level31ConstrainedBaseline];
  XCTAssertEqual(profileLevelId.profile, RTCH264ProfileConstrainedBaseline);
  XCTAssertEqual(profileLevelId.level, RTCH264Level3_1);
}

- (void)testInitWithProfileAndLevel {
  RTC_OBJC_TYPE(RTCH264ProfileLevelId) *profileLevelId =
      [[RTC_OBJC_TYPE(RTCH264ProfileLevelId) alloc] initWithProfile:RTCH264ProfileConstrainedHigh
                                                              level:RTCH264Level3_1];
  XCTAssertEqualObjects(profileLevelId.hexString, level31ConstrainedHigh);

  profileLevelId = [[RTC_OBJC_TYPE(RTCH264ProfileLevelId) alloc]
      initWithProfile:RTCH264ProfileConstrainedBaseline
                level:RTCH264Level3_1];
  XCTAssertEqualObjects(profileLevelId.hexString, level31ConstrainedBaseline);
}

@end

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

class RTCH264ProfileLevelIdTest : public XCTestToGTest<RTCH264ProfileLevelIdTests> {
 public:
  RTCH264ProfileLevelIdTest() = default;
  ~RTCH264ProfileLevelIdTest() override = default;
};

INVOKE_XCTEST(RTCH264ProfileLevelIdTest, InitWithString)

INVOKE_XCTEST(RTCH264ProfileLevelIdTest, InitWithProfileAndLevel)

}  // namespace webrtc

#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
