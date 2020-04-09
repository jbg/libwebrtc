/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCIceCandidate.h>
#import <WebRTC/RTCSessionDescription.h>

typedef enum {
  kARDSignalingMessageTypeCandidate,
  kARDSignalingMessageTypeCandidateRemoval,
  kARDSignalingMessageTypeOffer,
  kARDSignalingMessageTypeAnswer,
  kARDSignalingMessageTypeBye,
} ARDSignalingMessageType;

@interface ARDSignalingMessage : NSObject

@property(nonatomic, readonly) ARDSignalingMessageType type;

+ (ARDSignalingMessage *)messageFromJSONString:(NSString *)jsonString;
- (NSData *)JSONData;

@end

@interface ARDICECandidateMessage : ARDSignalingMessage

@property(nonatomic, readonly) WebRTCIceCandidate *candidate;

- (instancetype)initWithCandidate:(WebRTCIceCandidate *)candidate;

@end

@interface ARDICECandidateRemovalMessage : ARDSignalingMessage

@property(nonatomic, readonly) NSArray<WebRTCIceCandidate *> *candidates;

- (instancetype)initWithRemovedCandidates:(NSArray<WebRTCIceCandidate *> *)candidates;

@end

@interface ARDSessionDescriptionMessage : ARDSignalingMessage

@property(nonatomic, readonly) WebRTCSessionDescription *sessionDescription;

- (instancetype)initWithDescription:(WebRTCSessionDescription *)description;

@end

@interface ARDByeMessage : ARDSignalingMessage
@end
