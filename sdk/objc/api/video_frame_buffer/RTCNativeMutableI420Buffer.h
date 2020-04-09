/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>

#import "RTCMacros.h"
#import "RTCMutableI420Buffer.h"
#import "RTCNativeI420Buffer.h"

NS_ASSUME_NONNULL_BEGIN

/** Mutable version of WebRTCI420Buffer */
RTC_OBJC_EXPORT
@interface WebRTCMutableI420Buffer : WebRTCI420Buffer<WebRTCMutableI420Buffer>
@end

NS_ASSUME_NONNULL_END
