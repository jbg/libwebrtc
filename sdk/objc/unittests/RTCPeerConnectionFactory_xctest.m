/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/peerconnection/RTCAudioSource.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCDataChannel.h"
#import "api/peerconnection/RTCDataChannelConfiguration.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCMediaStreamTrack.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "api/peerconnection/RTCRtpReceiver.h"
#import "api/peerconnection/RTCRtpSender.h"
#import "api/peerconnection/RTCRtpTransceiver.h"
#import "api/peerconnection/RTCVideoSource.h"

#import <XCTest/XCTest.h>

@interface RTCPeerConnectionFactoryTests : XCTestCase
@end

@implementation RTCPeerConnectionFactoryTests

- (void)testPeerConnectionLifetime {
  @autoreleasepool {
    WebRTCConfiguration *config = [[WebRTCConfiguration alloc] init];

    WebRTCMediaConstraints *constraints =
        [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    WebRTCPeerConnectionFactory *factory;
    WebRTCPeerConnection *peerConnection;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [peerConnection close];
      factory = nil;
    }
    peerConnection = nil;
  }

  XCTAssertTrue(true, @"Expect test does not crash");
}

- (void)testMediaStreamLifetime {
  @autoreleasepool {
    WebRTCPeerConnectionFactory *factory;
    WebRTCMediaStream *mediaStream;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      mediaStream = [factory mediaStreamWithStreamId:@"mediaStream"];
      factory = nil;
    }
    mediaStream = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testDataChannelLifetime {
  @autoreleasepool {
    WebRTCConfiguration *config = [[WebRTCConfiguration alloc] init];
    WebRTCMediaConstraints *constraints =
        [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
    WebRTCDataChannelConfiguration *dataChannelConfig = [[WebRTCDataChannelConfiguration alloc] init];

    WebRTCPeerConnectionFactory *factory;
    WebRTCPeerConnection *peerConnection;
    WebRTCDataChannel *dataChannel;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      dataChannel =
          [peerConnection dataChannelForLabel:@"test_channel" configuration:dataChannelConfig];
      XCTAssertNotNil(dataChannel);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    dataChannel = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpTransceiverLifetime {
  @autoreleasepool {
    WebRTCConfiguration *config = [[WebRTCConfiguration alloc] init];
    config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
    WebRTCMediaConstraints *contraints =
        [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
    WebRTCRtpTransceiverInit *init = [[WebRTCRtpTransceiverInit alloc] init];

    WebRTCPeerConnectionFactory *factory;
    WebRTCPeerConnection *peerConnection;
    WebRTCRtpTransceiver *tranceiver;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      tranceiver = [peerConnection addTransceiverOfType:RTCRtpMediaTypeAudio init:init];
      XCTAssertNotNil(tranceiver);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    tranceiver = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpSenderLifetime {
  @autoreleasepool {
    WebRTCConfiguration *config = [[WebRTCConfiguration alloc] init];
    WebRTCMediaConstraints *constraints =
        [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    WebRTCPeerConnectionFactory *factory;
    WebRTCPeerConnection *peerConnection;
    WebRTCRtpSender *sender;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      sender = [peerConnection senderWithKind:kRTCMediaStreamTrackKindVideo streamId:@"stream"];
      XCTAssertNotNil(sender);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    sender = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpReceiverLifetime {
  @autoreleasepool {
    WebRTCConfiguration *config = [[WebRTCConfiguration alloc] init];
    WebRTCMediaConstraints *constraints =
        [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    WebRTCPeerConnectionFactory *factory;
    WebRTCPeerConnection *pc1;
    WebRTCPeerConnection *pc2;

    NSArray<WebRTCRtpReceiver *> *receivers1;
    NSArray<WebRTCRtpReceiver *> *receivers2;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      pc1 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc1 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      pc2 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc2 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      NSTimeInterval negotiationTimeout = 15;
      XCTAssertTrue([self negotiatePeerConnection:pc1
                               withPeerConnection:pc2
                               negotiationTimeout:negotiationTimeout]);

      XCTAssertEqual(pc1.signalingState, RTCSignalingStateStable);
      XCTAssertEqual(pc2.signalingState, RTCSignalingStateStable);

      receivers1 = pc1.receivers;
      receivers2 = pc2.receivers;
      XCTAssertTrue(receivers1.count > 0);
      XCTAssertTrue(receivers2.count > 0);
      [pc1 close];
      [pc2 close];
      pc1 = nil;
      pc2 = nil;
      factory = nil;
    }
    receivers1 = nil;
    receivers2 = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testAudioSourceLifetime {
  @autoreleasepool {
    WebRTCPeerConnectionFactory *factory;
    WebRTCAudioSource *audioSource;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      audioSource = [factory audioSourceWithConstraints:nil];
      XCTAssertNotNil(audioSource);
      factory = nil;
    }
    audioSource = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testVideoSourceLifetime {
  @autoreleasepool {
    WebRTCPeerConnectionFactory *factory;
    WebRTCVideoSource *videoSource;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      videoSource = [factory videoSource];
      XCTAssertNotNil(videoSource);
      factory = nil;
    }
    videoSource = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testAudioTrackLifetime {
  @autoreleasepool {
    WebRTCPeerConnectionFactory *factory;
    WebRTCAudioTrack *audioTrack;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      audioTrack = [factory audioTrackWithTrackId:@"audioTrack"];
      XCTAssertNotNil(audioTrack);
      factory = nil;
    }
    audioTrack = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testVideoTrackLifetime {
  @autoreleasepool {
    WebRTCPeerConnectionFactory *factory;
    WebRTCVideoTrack *videoTrack;

    @autoreleasepool {
      factory = [[WebRTCPeerConnectionFactory alloc] init];
      videoTrack = [factory videoTrackWithSource:[factory videoSource] trackId:@"videoTrack"];
      XCTAssertNotNil(videoTrack);
      factory = nil;
    }
    videoTrack = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (bool)negotiatePeerConnection:(WebRTCPeerConnection *)pc1
             withPeerConnection:(WebRTCPeerConnection *)pc2
             negotiationTimeout:(NSTimeInterval)timeout {
  __weak WebRTCPeerConnection *weakPC1 = pc1;
  __weak WebRTCPeerConnection *weakPC2 = pc2;
  WebRTCMediaConstraints *sdpConstraints =
      [[WebRTCMediaConstraints alloc] initWithMandatoryConstraints:@{
        kRTCMediaConstraintsOfferToReceiveAudio : kRTCMediaConstraintsValueTrue
      }
                                            optionalConstraints:nil];

  dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
  [weakPC1 offerForConstraints:sdpConstraints
             completionHandler:^(WebRTCSessionDescription *offer, NSError *error) {
               XCTAssertNil(error);
               XCTAssertNotNil(offer);
               [weakPC1
                   setLocalDescription:offer
                     completionHandler:^(NSError *error) {
                       XCTAssertNil(error);
                       [weakPC2
                           setRemoteDescription:offer
                              completionHandler:^(NSError *error) {
                                XCTAssertNil(error);
                                [weakPC2
                                    answerForConstraints:sdpConstraints
                                       completionHandler:^(WebRTCSessionDescription *answer,
                                                           NSError *error) {
                                         XCTAssertNil(error);
                                         XCTAssertNotNil(answer);
                                         [weakPC2
                                             setLocalDescription:answer
                                               completionHandler:^(NSError *error) {
                                                 XCTAssertNil(error);
                                                 [weakPC1
                                                     setRemoteDescription:answer
                                                        completionHandler:^(NSError *error) {
                                                          XCTAssertNil(error);
                                                          dispatch_semaphore_signal(negotiatedSem);
                                                        }];
                                               }];
                                       }];
                              }];
                     }];
             }];

  return 0 ==
      dispatch_semaphore_wait(negotiatedSem,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)));
}

@end
