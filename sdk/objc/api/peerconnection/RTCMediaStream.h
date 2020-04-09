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

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class WebRTCAudioTrack;
@class WebRTCPeerConnectionFactory;
@class WebRTCVideoTrack;

RTC_OBJC_EXPORT
@interface WebRTCMediaStream : NSObject

/** The audio tracks in this stream. */
@property(nonatomic, strong, readonly) NSArray<WebRTCAudioTrack *> *audioTracks;

/** The video tracks in this stream. */
@property(nonatomic, strong, readonly) NSArray<WebRTCVideoTrack *> *videoTracks;

/** An identifier for this media stream. */
@property(nonatomic, readonly) NSString *streamId;

- (instancetype)init NS_UNAVAILABLE;

/** Adds the given audio track to this media stream. */
- (void)addAudioTrack:(WebRTCAudioTrack *)audioTrack;

/** Adds the given video track to this media stream. */
- (void)addVideoTrack:(WebRTCVideoTrack *)videoTrack;

/** Removes the given audio track to this media stream. */
- (void)removeAudioTrack:(WebRTCAudioTrack *)audioTrack;

/** Removes the given video track to this media stream. */
- (void)removeVideoTrack:(WebRTCVideoTrack *)videoTrack;

@end

NS_ASSUME_NONNULL_END
