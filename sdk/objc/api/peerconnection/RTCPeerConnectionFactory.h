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

@class WebRTCAudioSource;
@class WebRTCAudioTrack;
@class WebRTCConfiguration;
@class WebRTCMediaConstraints;
@class WebRTCMediaStream;
@class WebRTCPeerConnection;
@class WebRTCVideoSource;
@class WebRTCVideoTrack;
@class WebRTCPeerConnectionFactoryOptions;
@protocol WebRTCPeerConnectionDelegate;
@protocol WebRTCVideoDecoderFactory;
@protocol WebRTCVideoEncoderFactory;

RTC_OBJC_EXPORT
@interface WebRTCPeerConnectionFactory : NSObject

/* Initialize object with default H264 video encoder/decoder factories */
- (instancetype)init;

/* Initialize object with injectable video encoder/decoder factories */
- (instancetype)initWithEncoderFactory:(nullable id<WebRTCVideoEncoderFactory>)encoderFactory
                        decoderFactory:(nullable id<WebRTCVideoDecoderFactory>)decoderFactory;

/** Initialize an WebRTCAudioSource with constraints. */
- (WebRTCAudioSource *)audioSourceWithConstraints:(nullable WebRTCMediaConstraints *)constraints;

/** Initialize an WebRTCAudioTrack with an id. Convenience ctor to use an audio source with no
 *  constraints.
 */
- (WebRTCAudioTrack *)audioTrackWithTrackId:(NSString *)trackId;

/** Initialize an WebRTCAudioTrack with a source and an id. */
- (WebRTCAudioTrack *)audioTrackWithSource:(WebRTCAudioSource *)source trackId:(NSString *)trackId;

/** Initialize a generic WebRTCVideoSource. The WebRTCVideoSource should be passed to a WebRTCVideoCapturer
 *  implementation, e.g. WebRTCCameraVideoCapturer, in order to produce frames.
 */
- (WebRTCVideoSource *)videoSource;

/** Initialize an WebRTCVideoTrack with a source and an id. */
- (WebRTCVideoTrack *)videoTrackWithSource:(WebRTCVideoSource *)source trackId:(NSString *)trackId;

/** Initialize an WebRTCMediaStream with an id. */
- (WebRTCMediaStream *)mediaStreamWithStreamId:(NSString *)streamId;

/** Initialize an WebRTCPeerConnection with a configuration, constraints, and
 *  delegate.
 */
- (WebRTCPeerConnection *)peerConnectionWithConfiguration:(WebRTCConfiguration *)configuration
                                           constraints:(WebRTCMediaConstraints *)constraints
                                              delegate:
                                                  (nullable id<WebRTCPeerConnectionDelegate>)delegate;

/** Set the options to be used for subsequently created RTCPeerConnections */
- (void)setOptions:(nonnull WebRTCPeerConnectionFactoryOptions *)options;

/** Start an AecDump recording. This API call will likely change in the future. */
- (BOOL)startAecDumpWithFilePath:(NSString *)filePath maxSizeInBytes:(int64_t)maxSizeInBytes;

/* Stop an active AecDump recording */
- (void)stopAecDump;

@end

NS_ASSUME_NONNULL_END
