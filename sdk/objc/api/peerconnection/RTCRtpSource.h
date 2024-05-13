/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
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

/** Represents the media type of the RtpReceiver. */
typedef NS_ENUM(NSInteger, RTCRtpSourceType) {
  RTCRtpSourceTypeSSRC,
  RTCRtpSourceTypeCSRC,
};

@class RTC_OBJC_TYPE(RTCRtpSource);

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCRtpSource)<NSObject>

    @property(nonatomic, readonly) uint32_t sourceId;

@property(nonatomic, readonly) RTCRtpSourceType sourceType;

@property(nonatomic, readonly, nullable) NSNumber *audioLevel;

@property(nonatomic, readonly) uint64_t timestampUs;

@property(nonatomic, readonly) uint32_t rtpTimestamp;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCRtpSource) : NSObject <RTC_OBJC_TYPE(RTCRtpSource)>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
