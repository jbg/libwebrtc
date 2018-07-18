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

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@interface RTCCertificate : NSObject <NSCopying>

/** Private key in PEM. */
@property(nonatomic, readonly) NSString *private_key;

/** Public key in an x509 cert encoded in PEM. */
@property(nonatomic, readonly) NSString *certificate;

/**
 * Initialize an RTCCertificate with PEM strings for private_key and certificate.
 */
- (instancetype)initWithStrings:(NSString *)private_key
                    certificate:(NSString *)certificate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
