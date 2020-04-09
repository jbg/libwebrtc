/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCDtmfSender.h"
#import "RTCMacros.h"
#import "RTCMediaStreamTrack.h"
#import "RTCRtpParameters.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol WebRTCRtpSender <NSObject>

/** A unique identifier for this sender. */
@property(nonatomic, readonly) NSString *senderId;

/** The currently active WebRTCRtpParameters, as defined in
 *  https://www.w3.org/TR/webrtc/#idl-def-WebRTCRtpParameters.
 */
@property(nonatomic, copy) WebRTCRtpParameters *parameters;

/** The WebRTCMediaStreamTrack associated with the sender.
 *  Note: reading this property returns a new instance of
 *  WebRTCMediaStreamTrack. Use isEqual: instead of == to compare
 *  WebRTCMediaStreamTrack instances.
 */
@property(nonatomic, copy, nullable) WebRTCMediaStreamTrack *track;

/** IDs of streams associated with the RTP sender */
@property(nonatomic, copy) NSArray<NSString *> *streamIds;

/** The WebRTCDtmfSender accociated with the RTP sender. */
@property(nonatomic, readonly, nullable) id<WebRTCDtmfSender> dtmfSender;

@end

RTC_OBJC_EXPORT
@interface WebRTCRtpSender : NSObject <WebRTCRtpSender>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
