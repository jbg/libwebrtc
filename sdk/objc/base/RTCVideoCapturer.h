/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoFrame.h"

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class WebRTCVideoCapturer;

RTC_OBJC_EXPORT
@protocol WebRTCVideoCapturerDelegate <NSObject>
- (void)capturer:(WebRTCVideoCapturer *)capturer didCaptureVideoFrame:(WebRTCVideoFrame *)frame;
@end

RTC_OBJC_EXPORT
@interface WebRTCVideoCapturer : NSObject

@property(nonatomic, weak) id<WebRTCVideoCapturerDelegate> delegate;

- (instancetype)initWithDelegate:(id<WebRTCVideoCapturerDelegate>)delegate;

@end

NS_ASSUME_NONNULL_END
