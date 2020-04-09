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

#import "RTCMacros.h"
#import "RTCMediaStreamTrack.h"
#import "RTCRtpParameters.h"

NS_ASSUME_NONNULL_BEGIN

/** Represents the media type of the RtpReceiver. */
typedef NS_ENUM(NSInteger, RTCRtpMediaType) {
  RTCRtpMediaTypeAudio,
  RTCRtpMediaTypeVideo,
  RTCRtpMediaTypeData,
};

@class WebRTCRtpReceiver;

RTC_OBJC_EXPORT
@protocol WebRTCRtpReceiverDelegate <NSObject>

/** Called when the first RTP packet is received.
 *
 *  Note: Currently if there are multiple RtpReceivers of the same media type,
 *  they will all call OnFirstPacketReceived at once.
 *
 *  For example, if we create three audio receivers, A/B/C, they will listen to
 *  the same signal from the underneath network layer. Whenever the first audio packet
 *  is received, the underneath signal will be fired. All the receivers A/B/C will be
 *  notified and the callback of the receiver's delegate will be called.
 *
 *  The process is the same for video receivers.
 */
- (void)rtpReceiver:(WebRTCRtpReceiver *)rtpReceiver
    didReceiveFirstPacketForMediaType:(RTCRtpMediaType)mediaType;

@end

RTC_OBJC_EXPORT
@protocol WebRTCRtpReceiver <NSObject>

/** A unique identifier for this receiver. */
@property(nonatomic, readonly) NSString *receiverId;

/** The currently active WebRTCRtpParameters, as defined in
 *  https://www.w3.org/TR/webrtc/#idl-def-WebRTCRtpParameters.
 *
 *  The WebRTC specification only defines WebRTCRtpParameters in terms of senders,
 *  but this API also applies them to receivers, similar to ORTC:
 *  http://ortc.org/wp-content/uploads/2016/03/ortc.html#rtcrtpparameters*.
 */
@property(nonatomic, readonly) WebRTCRtpParameters *parameters;

/** The WebRTCMediaStreamTrack associated with the receiver.
 *  Note: reading this property returns a new instance of
 *  WebRTCMediaStreamTrack. Use isEqual: instead of == to compare
 *  WebRTCMediaStreamTrack instances.
 */
@property(nonatomic, readonly, nullable) WebRTCMediaStreamTrack *track;

/** The delegate for this RtpReceiver. */
@property(nonatomic, weak) id<WebRTCRtpReceiverDelegate> delegate;

@end

RTC_OBJC_EXPORT
@interface WebRTCRtpReceiver : NSObject <WebRTCRtpReceiver>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
