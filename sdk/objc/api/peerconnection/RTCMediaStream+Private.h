/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaStream.h"

#include "api/media_stream_interface.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebRTCMediaStream ()

/**
 * MediaStreamInterface representation of this WebRTCMediaStream object. This is
 * needed to pass to the underlying C++ APIs.
 */
@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::MediaStreamInterface> nativeMediaStream;

/** Initialize an WebRTCMediaStream with an id. */
- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory streamId:(NSString *)streamId;

/** Initialize an WebRTCMediaStream from a native MediaStreamInterface. */
- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory
              nativeMediaStream:(rtc::scoped_refptr<webrtc::MediaStreamInterface>)nativeMediaStream;

@end

NS_ASSUME_NONNULL_END
