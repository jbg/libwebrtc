/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpTransceiver.h"

#include "api/rtp_transceiver_interface.h"

NS_ASSUME_NONNULL_BEGIN

@class WebRTCPeerConnectionFactory;

@interface WebRTCRtpTransceiverInit ()

@property(nonatomic, readonly) webrtc::RtpTransceiverInit nativeInit;

@end

@interface WebRTCRtpTransceiver ()

@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::RtpTransceiverInterface>
    nativeRtpTransceiver;

/** Initialize an WebRTCRtpTransceiver with a native RtpTransceiverInterface. */
- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory*)factory
           nativeRtpTransceiver:
               (rtc::scoped_refptr<webrtc::RtpTransceiverInterface>)nativeRtpTransceiver
    NS_DESIGNATED_INITIALIZER;

+ (webrtc::RtpTransceiverDirection)nativeRtpTransceiverDirectionFromDirection:
        (RTCRtpTransceiverDirection)direction;

+ (RTCRtpTransceiverDirection)rtpTransceiverDirectionFromNativeDirection:
        (webrtc::RtpTransceiverDirection)nativeDirection;

@end

NS_ASSUME_NONNULL_END
