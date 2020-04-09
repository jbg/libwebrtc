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
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class WebRTCVideoFrame;

RTC_OBJC_EXPORT
@protocol WebRTCVideoRenderer <NSObject>

/** The size of the frame. */
- (void)setSize:(CGSize)size;

/** The frame to be displayed. */
- (void)renderFrame:(nullable WebRTCVideoFrame *)frame;

@end

RTC_OBJC_EXPORT
@protocol WebRTCVideoViewDelegate

- (void)videoView:(id<WebRTCVideoRenderer>)videoView didChangeVideoSize:(CGSize)size;

@end

NS_ASSUME_NONNULL_END
