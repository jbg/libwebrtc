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
@interface RTCSslConfig : NSObject

/** Indicates whether to enable the OCSP stapling in TLS. */
@property(nonatomic, assign) BOOL enableOcspStapling;

/** Indicates whether to enable the signed certificate timestamp extension in TLS. */
@property(nonatomic, assign) BOOL enableSignedCertTimestamp;

/** Indicates whether to enable the TLS Channel ID extension. */
@property(nonatomic, assign) BOOL enableTlsChannelId;

/** Indicates whether to enable the TLS GREASE extension. */
@property(nonatomic, assign) BOOL enableGrease;

/** Highest supportedSL version, as defined in the supported_groups TLS extension. */
@property(nonatomic, assign, nullable) NSNumber *maxSslVersion;

/** List of protocols to be used in the TLS ALPN extension. */
@property(nonatomic, copy, nullable) NSArray<NSString *> *tlsAlpnProtocols;

/**
 List of elliptic curves to be used in the TLS elliptic curves extension.
 Only curve names supported by OpenSSL should be used (eg. "P-256","X25519").
 */
@property(nonatomic, copy, nullable) NSArray<NSString *> *tlsEllipticCurves;

- (instancetype)init;

@end

NS_ASSUME_NONNULL_END
