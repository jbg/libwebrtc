/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecH264.h"

#include <vector>

#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodec.h"

#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"
#include "system_wrappers/include/field_trial.h"

#import <VideoToolbox/VideoToolbox.h>

const char kHighProfileExperiment[] = "WebRTC-H264HighProfile";

bool IsHighProfileEnabled() {
  return webrtc::field_trial::IsEnabled(kHighProfileExperiment);
}

// H264 specific settings.
@implementation RTCCodecSpecificInfoH264

@synthesize packetizationMode = _packetizationMode;

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo codecSpecificInfo;
  codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  codecSpecificInfo.codec_name = [kRTCVideoCodecH264Name cStringUsingEncoding:NSUTF8StringEncoding];
  codecSpecificInfo.codecSpecific.H264.packetization_mode =
      (webrtc::H264PacketizationMode)_packetizationMode;

  return codecSpecificInfo;
}

@end

// Encoder factory.
@implementation RTCVideoEncoderFactoryH264

static NSString *bestConstrainedHighProfile;
static NSString *bestConstrainedBaselineProfile;
static BOOL initialized;
static dispatch_block_t initializeBlock;
static dispatch_semaphore_t waitForInitializeSemaphore;

+ (void)initialize {
  RTC_LOG(LS_INFO) << "Load method called for WebRtcVideoEncoderFactory";
  waitForInitializeSemaphore = dispatch_semaphore_create(1);
  initialized = NO;
  initializeBlock = dispatch_block_create(DISPATCH_BLOCK_INHERIT_QOS_CLASS, ^{
    // Auto determine supported h264 levels
    NSDate *autoTimeStart = [NSDate date];
    VTCompressionSessionRef session;
    VTCompressionSessionCreate(
        NULL, 120, 120, kCMVideoCodecType_H264, NULL, NULL, NULL, NULL, NULL, &session);
    CFDictionaryRef properties;
    VTSessionCopySupportedPropertyDictionary(session, &properties);
    NSDictionary *propertiesDict = (__bridge NSDictionary *)properties;
    NSDictionary *profileLevels =
        [propertiesDict objectForKey:(NSString *)kVTCompressionPropertyKey_ProfileLevel];
    NSArray *supportedLevels =
        [profileLevels objectForKey:(NSString *)kVTPropertySupportedValueListKey];

    NSString *constrainedHighProfile = kRTCLevel31ConstrainedHigh;
    NSString *constrainedBaselineProfile = kRTCLevel31ConstrainedBaseline;

    for (NSString *level in supportedLevels) {
      RTC_LOG(LS_INFO) << "Profile " << [level UTF8String];
      CFStringRef cfLevel = (__bridge CFStringRef)level;
      if (cfLevel == kVTProfileLevel_H264_High_5_2) {
        constrainedHighProfile = kRTCLevel52ConstrainedHigh;
      } else if (cfLevel == kVTProfileLevel_H264_High_4_1) {
        constrainedHighProfile = kRTCLevel41ConstrainedHigh;
      } else if (cfLevel == kVTProfileLevel_H264_Baseline_5_2) {
        constrainedBaselineProfile = kRTCLevel52ConstrainedBaseline;
      } else if (cfLevel == kVTProfileLevel_H264_Baseline_4_1) {
        constrainedBaselineProfile = kRTCLevel41ConstrainedBaseline;
      }
    }

    if (session) {
      VTCompressionSessionInvalidate(session);
      CFRelease(session);
    }

    bestConstrainedHighProfile = constrainedHighProfile;
    bestConstrainedBaselineProfile = constrainedBaselineProfile;

    NSDate *autoTimeFinish = [NSDate date];
    NSTimeInterval executionTime = [autoTimeFinish timeIntervalSinceDate:autoTimeStart];
    RTC_LOG(LS_INFO) << "Determined that best available h264 high profile is: "
                     << [constrainedHighProfile UTF8String];
    RTC_LOG(LS_INFO) << "Auto determining h264 profile took " << executionTime * 1000 << "ms";
    initialized = YES;
  });

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), initializeBlock);
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSDate *methodStart = [NSDate date];
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [NSMutableArray array];
  NSString *codecName = kRTCVideoCodecH264Name;

  if (!initialized) {
    dispatch_semaphore_wait(waitForInitializeSemaphore, DISPATCH_TIME_FOREVER);
    if (!initialized) {
      long ret = dispatch_block_wait(initializeBlock, DISPATCH_TIME_FOREVER);
      RTC_DCHECK_EQ(ret, 0);
    }
    dispatch_semaphore_signal(waitForInitializeSemaphore);
  }

  if (IsHighProfileEnabled()) {
    NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
      @"profile-level-id" : bestConstrainedHighProfile,
      @"level-asymmetry-allowed" : @"1",
      @"packetization-mode" : @"1",
    };
    RTCVideoCodecInfo *constrainedHighInfo =
        [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:constrainedHighParams];
    [codecs addObject:constrainedHighInfo];
  }

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : bestConstrainedBaselineProfile,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedBaselineInfo =
      [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:constrainedBaselineParams];
  [codecs addObject:constrainedBaselineInfo];

  NSDate *methodFinish = [NSDate date];
  NSTimeInterval execTime = [methodFinish timeIntervalSinceDate:methodStart];
  RTC_LOG(LS_INFO) << "Whole supported codecs took " << execTime * 1000 << "ms";

  return [codecs copy];
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
}

@end

// Decoder factory.
@implementation RTCVideoDecoderFactoryH264

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoDecoderH264 alloc] init];
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSString *codecName = kRTCVideoCodecH264Name;
  return @[ [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:nil] ];
}

@end
