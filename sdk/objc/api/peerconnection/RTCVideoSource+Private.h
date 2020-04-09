/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoSource.h"

#import "RTCMediaSource+Private.h"

#include "api/media_stream_interface.h"
#include "rtc_base/thread.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebRTCVideoSource ()

/**
 * The VideoTrackSourceInterface object passed to this WebRTCVideoSource during
 * construction.
 */
@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>
    nativeVideoSource;

/** Initialize an WebRTCVideoSource from a native VideoTrackSourceInterface. */
- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory
              nativeVideoSource:
                  (rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>)nativeVideoSource
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory
              nativeMediaSource:(rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource
                           type:(RTCMediaSourceType)type NS_UNAVAILABLE;

- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory
                signalingThread:(rtc::Thread *)signalingThread
                   workerThread:(rtc::Thread *)workerThread;

@end

NS_ASSUME_NONNULL_END
