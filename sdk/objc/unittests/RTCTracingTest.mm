/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <vector>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCTracing.h"
#import "helpers/NSString+StdString.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "sdk/objc/unittests/xctest_to_gtest.h"
#endif

@interface RTCTracingTests : XCTestCase
@end

@implementation RTCTracingTests

- (NSString *)documentsFilePathForFileName:(NSString *)fileName {
  NSParameterAssert(fileName.length);
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = paths.firstObject;
  NSString *filePath =
  [documentsDirPath stringByAppendingPathComponent:fileName];
  return filePath;
}

- (void)testTracingTestNoInitialization {
  NSString *filePath = [self documentsFilePathForFileName:@"webrtc-trace.txt"];
  EXPECT_EQ(NO, RTCStartInternalCapture(filePath));
  RTCStopInternalCapture();
}

@end

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

class RTCTracingTest : public XCTestToGTest<RTCTracingTests> {
 public:
  RTCTracingTest() = default;
  ~RTCTracingTest() override = default;
};

INVOKE_XCTEST(RTCTracingTest, TracingTestNoInitialization)

}  // namespace webrtc

#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
