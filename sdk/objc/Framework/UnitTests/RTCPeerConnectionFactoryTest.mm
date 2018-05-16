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

#include <vector>

#include "rtc_base/gunit.h"

#import "WebRTC/RTCConfiguration.h"
#import "WebRTC/RTCMediaConstraints.h"
#import "WebRTC/RTCPeerConnection.h"
#import "WebRTC/RTCPeerConnectionFactory.h"

@interface RTCPeerConnectionFactoryTest : NSObject
- (void)testPeerConnectionLifetime;
- (void)testMediaStreamLifetime;
- (void)testVideoSourceLifetime;
- (void)testAudioTrackLifetime;
- (void)testVideoTrackLifetime;
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

- (void)testMediaStreamLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

    RTCMediaStream *mediaStream = [factory mediaStreamWithStreamId:@"mediaStream"];

    mediaStream = nil;
    factory = nil;
  }

  EXPECT_TRUE(true) << "Expect test does not crash";
}

- (void)testVideoSourceLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

    RTCVideoSource *videoSource = [factory videoSource];

    videoSource = nil;
    factory = nil;
  }

  EXPECT_TRUE(true) << "Expect test does not crash";
}

- (void)testAudioTrackLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

    RTCAudioTrack *audioTrack = [factory audioTrackWithTrackId:@"audioTrack"];

    audioTrack = nil;
    factory = nil;
  }

  EXPECT_TRUE(true) << "Expect test does not crash";
}

- (void)testVideoTrackLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

    RTCVideoTrack *videoTrack =
        [factory videoTrackWithSource:[factory videoSource] trackId:@"videoTrack"];

    videoTrack = nil;
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

TEST(RTCPeerConnectionFactoryTest, RTCAudioTrackLifetimeTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryTest *test = [RTCPeerConnectionFactoryTest new];
    [test testAudioTrackLifetime];
  }
}

TEST(RTCPeerConnectionFactoryTest, RTCVideoSourceLifetimeTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryTest *test = [RTCPeerConnectionFactoryTest new];
    [test testVideoSourceLifetime];
  }
}

TEST(RTCPeerConnectionFactoryTest, RTCVideoTrackLifetimeTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryTest *test = [RTCPeerConnectionFactoryTest new];
    [test testVideoTrackLifetime];
  }
}

TEST(RTCPeerConnectionFactoryTest, RTCMediaStreamLifetimeTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryTest *test = [RTCPeerConnectionFactoryTest new];
    [test testMediaStreamLifetime];
  }
}
