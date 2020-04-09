/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaStreamTrack.h"

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@protocol WebRTCVideoRenderer;
@class WebRTCPeerConnectionFactory;
@class WebRTCVideoSource;

RTC_OBJC_EXPORT
@interface WebRTCVideoTrack : WebRTCMediaStreamTrack

/** The video source for this video track. */
@property(nonatomic, readonly) WebRTCVideoSource *source;

- (instancetype)init NS_UNAVAILABLE;

/** Register a renderer that will render all frames received on this track. */
- (void)addRenderer:(id<WebRTCVideoRenderer>)renderer;

/** Deregister a renderer. */
- (void)removeRenderer:(id<WebRTCVideoRenderer>)renderer;

@end

NS_ASSUME_NONNULL_END
