/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCLogging.h>

NS_ASSUME_NONNULL_BEGIN


// This class intercepts WebRTC logs and forwards them to a registered block.
// This class is not threadsafe.
RTC_EXPORT
@interface RTCCallbackLogger : NSObject

// The severity level to capture. The default is kRTCLoggingSeverityInfo.
@property(nonatomic, assign) RTCLoggingSeverity severity;

- (void)start:(nullable void (^)(NSString*))callback;
- (void)stop;

@end

NS_ASSUME_NONNULL_END

