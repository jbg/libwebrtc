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

#import "NSString+StdString.h"
#import "RTCConfiguration+Private.h"
#import "WebRTC/RTCConfiguration.h"
#import "WebRTC/RTCIceServer.h"
#import "WebRTC/RTCMediaConstraints.h"
#import "WebRTC/RTCPeerConnection.h"
#import "WebRTC/RTCPeerConnectionFactory.h"

@interface RTCCertificateTest : NSObject
- (void)testCertificateLifecycle;
@end

@implementation RTCCertificateTest

- (void)testCertificateLifecycle {
  RTCConfiguration *config = [[RTCConfiguration alloc] init];

  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];
  config.iceServers = @[ server ];

  RTCCertificate *originalCertificate = [config generateCertificateWithParams:@{
    @"expires" : @100000,
    @"name" : @"RSASSA-PKCS1-v1_5"
  }];
  config.certificate = originalCertificate;

  RTCMediaConstraints *contraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
  RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

  RTCConfiguration *newConfig;
  @autoreleasepool {
    RTCPeerConnection *peerConnection =
        [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
    newConfig = peerConnection.configuration;
  }

  std::string originalPrivateKeyField = [[originalCertificate private_key] UTF8String];
  std::string originalCertificateField = [[originalCertificate certificate] UTF8String];

  RTCCertificate *retrievedCertificate = newConfig.certificate;
  std::string retrievedPrivateKeyField = [[retrievedCertificate private_key] UTF8String];
  std::string retrievedCertificateField = [[retrievedCertificate certificate] UTF8String];

  EXPECT_EQ(originalPrivateKeyField, retrievedPrivateKeyField);
  EXPECT_EQ(retrievedCertificateField, retrievedCertificateField);
}

@end

TEST(RTCCertificateTest, CertificateLifecycleTest) {
  @autoreleasepool {
    RTCCertificateTest *test = [[RTCCertificateTest alloc] init];
    [test testCertificateLifecycle];
  }
}
