/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCCertificate.h>

@implementation RTCCertificate

@synthesize private_key = _private_key;
@synthesize certificate = _certificate;

- (id)copyWithZone:(NSZone *)zone {
  id copy = [[[self class] alloc] initWithStrings:[self.private_key copyWithZone:zone]
                                      certificate:[self.certificate copyWithZone:zone]];
  return copy;
}

- (instancetype)initWithStrings:(NSString *)private_key certificate:(NSString *)certificate {
  if (self = [super init]) {
    _private_key = [private_key copy];
    _certificate = [certificate copy];
  }
  return self;
}

@end
