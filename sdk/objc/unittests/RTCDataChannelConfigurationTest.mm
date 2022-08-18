/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCDataChannelConfiguration+Private.h"
#import "api/peerconnection/RTCDataChannelConfiguration.h"
#import "helpers/NSString+StdString.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "sdk/objc/unittests/xctest_to_gtest.h"
#endif

@interface RTCDataChannelConfigurationTests : XCTestCase
@end

@implementation RTCDataChannelConfigurationTests

- (void)testConversionToNativeDataChannelInit {
  BOOL isOrdered = NO;
  int maxPacketLifeTime = 5;
  int maxRetransmits = 4;
  BOOL isNegotiated = YES;
  int channelId = 4;
  NSString *protocol = @"protocol";

  RTC_OBJC_TYPE(RTCDataChannelConfiguration) *dataChannelConfig =
      [[RTC_OBJC_TYPE(RTCDataChannelConfiguration) alloc] init];
  dataChannelConfig.isOrdered = isOrdered;
  dataChannelConfig.maxPacketLifeTime = maxPacketLifeTime;
  dataChannelConfig.maxRetransmits = maxRetransmits;
  dataChannelConfig.isNegotiated = isNegotiated;
  dataChannelConfig.channelId = channelId;
  dataChannelConfig.protocol = protocol;

  webrtc::DataChannelInit nativeInit = dataChannelConfig.nativeDataChannelInit;
  EXPECT_EQ(isOrdered, nativeInit.ordered);
  EXPECT_EQ(maxPacketLifeTime, nativeInit.maxRetransmitTime);
  EXPECT_EQ(maxRetransmits, nativeInit.maxRetransmits);
  EXPECT_EQ(isNegotiated, nativeInit.negotiated);
  EXPECT_EQ(channelId, nativeInit.id);
  EXPECT_EQ(protocol.stdString, nativeInit.protocol);
}

@end

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

class RTCDataChannelConfigurationTest : public XCTestToGTest<RTCDataChannelConfigurationTests> {
 public:
  RTCDataChannelConfigurationTest() = default;
  ~RTCDataChannelConfigurationTest() override = default;
};

INVOKE_XCTEST(RTCDataChannelConfigurationTest, ConversionToNativeDataChannelInit)

}  // namespace webrtc

#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
