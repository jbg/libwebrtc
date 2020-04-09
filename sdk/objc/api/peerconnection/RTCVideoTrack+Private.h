/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoTrack.h"

#include "api/media_stream_interface.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebRTCVideoTrack ()

/** VideoTrackInterface created or passed in at construction. */
@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::VideoTrackInterface> nativeVideoTrack;

/** Initialize an WebRTCVideoTrack with its source and an id. */
- (instancetype)initWithFactory:(WebRTCPeerConnectionFactory *)factory
                         source:(WebRTCVideoSource *)source
                        trackId:(NSString *)trackId;

@end

NS_ASSUME_NONNULL_END
