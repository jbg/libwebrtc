/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "rtc_base/gunit.h"

#import "WebRTC/RTCConfiguration.h"
#import "WebRTC/RTCMediaConstraints.h"
#import "WebRTC/RTCPeerConnection.h"
#import "WebRTC/RTCPeerConnectionFactory.h"

@interface RTCPeerConnectionFactoryTest : NSObject
- (void)testPeerConnectionLifetime;
@end

@implementation RTCPeerConnectionFactoryTest

- (void)testPeerConnectionLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];

    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

    RTCPeerConnection *peerConnection =
        [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];

    peerConnection = nil;
    factory = nil;
  }

  EXPECT_TRUE(true) << "Expect test does not crash";
}

@end

TEST(RTCPeerConnectionFactoryTest, RTCPeerConnectionLifetimeTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryTest *test = [RTCPeerConnectionFactoryTest new];
    [test testPeerConnectionLifetime];
  }
}