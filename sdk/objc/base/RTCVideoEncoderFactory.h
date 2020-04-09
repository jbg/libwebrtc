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
#import "RTCVideoEncoder.h"

NS_ASSUME_NONNULL_BEGIN

/** WebRTCVideoEncoderFactory is an Objective-C version of
 webrtc::VideoEncoderFactory::VideoEncoderSelector.
 */
RTC_OBJC_EXPORT
@protocol WebRTCVideoEncoderSelector <NSObject>

- (void)registerCurrentEncoderInfo:(WebRTCVideoCodecInfo *)info;
- (nullable WebRTCVideoCodecInfo *)encoderForBitrate:(NSInteger)bitrate;
- (nullable WebRTCVideoCodecInfo *)encoderForBrokenEncoder;

@end

/** WebRTCVideoEncoderFactory is an Objective-C version of webrtc::VideoEncoderFactory. */
RTC_OBJC_EXPORT
@protocol WebRTCVideoEncoderFactory <NSObject>

- (nullable id<WebRTCVideoEncoder>)createEncoder:(WebRTCVideoCodecInfo *)info;
- (NSArray<WebRTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@optional
- (NSArray<WebRTCVideoCodecInfo *> *)implementations;
- (nullable id<WebRTCVideoEncoderSelector>)encoderSelector;

@end

NS_ASSUME_NONNULL_END
