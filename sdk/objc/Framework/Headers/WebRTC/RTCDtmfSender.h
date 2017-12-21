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

#import <WebRTC/RTCMacros.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@protocol RTCDtmfSender <NSObject>

/** Returns true if this DtmfSender is capable of sending DTMF */
@property(nonatomic, readonly) BOOL canInsertDtmf;

/**
 * Queues a task that sends the provided DTMF tones.
 * If InsertDtmf is called on the same object while an existing task for this
 * object to generate DTMF is still running, the previous task is canceled.
 * Returns true on success and false on failure.
 */
- (BOOL)insertDtmf:(nonnull NSString *)tones
          duration:(NSTimeInterval)duration
      interToneGap:(NSTimeInterval)interToneGap;

/** The tones remaining to be played out */
- (nonnull NSString *)remainingTones;

/**
 * The current tone duration value. This value will be the value last set via the
 * insertDtmf method, or the default value of 100 ms if insertDtmf was never called.
 */
- (NSTimeInterval)duration;

/**
 * The current value of the between-tone gap. This value will be the value last set
 * via the insertDtmf() method, or the default value of 50 ms if insertDtmf() was never
 * called.
 */
- (NSTimeInterval)interToneGap;

@end

RTC_EXPORT
@interface RTCDtmfSender : NSObject <RTCDtmfSender>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
