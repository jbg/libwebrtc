/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCodecInfo.h"
#import "RTCVideoDecoder.h"

NS_ASSUME_NONNULL_BEGIN

/** WebRTCVideoDecoderFactory is an Objective-C version of webrtc::VideoDecoderFactory. */
RTC_OBJC_EXPORT
@protocol WebRTCVideoDecoderFactory <NSObject>

- (nullable id<WebRTCVideoDecoder>)createDecoder:(WebRTCVideoCodecInfo *)info;
- (NSArray<WebRTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@end

NS_ASSUME_NONNULL_END
