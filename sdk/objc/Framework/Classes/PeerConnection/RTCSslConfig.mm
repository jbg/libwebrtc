/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCSslConfig+Private.h"

#import "NSString+StdString.h"

@implementation RTCSslConfig

@synthesize enableOcspStapling = _enableOcspStapling;
@synthesize enableSignedCertTimestamp = _enableSignedCertTimestamp;
@synthesize enableTlsChannelId = _enableTlsChannelId;
@synthesize enableGrease = _enableGrease;
@synthesize maxSslVersion = _maxSslVersion;
@synthesize tlsAlpnProtocols = _tlsAlpnProtocols;
@synthesize tlsEllipticCurves = _tlsEllipticCurves;

- (instancetype)init {
  // Copy defaults
  webrtc::PeerConnectionInterface::SSLConfig config;
  return [self initWithNativeConfig:config];
}

- (instancetype)initWithNativeConfig:(const webrtc::PeerConnectionInterface::SSLConfig &)config {
  if (self = [super init]) {
    _enableOcspStapling = config.enable_ocsp_stapling;
    _enableSignedCertTimestamp = config.enable_signed_cert_timestamp;
    _enableTlsChannelId = config.enable_tls_channel_id;
    _enableGrease = config.enable_grease;
    if (config.max_ssl_version) {
      _maxSslVersion = [NSNumber numberWithInt:*config.max_ssl_version];
    }
    if (config.tls_alpn_protocols) {
      NSMutableArray *tlsAlpnProtocols =
          [NSMutableArray arrayWithCapacity:config.tls_alpn_protocols.value().size()];
      for (auto const &proto : config.tls_alpn_protocols.value()) {
        [tlsAlpnProtocols addObject:[NSString stringForStdString:proto]];
      }
      _tlsAlpnProtocols = tlsAlpnProtocols;
    }
    if (config.tls_elliptic_curves) {
      NSMutableArray *tlsEllipticCurves =
          [NSMutableArray arrayWithCapacity:config.tls_elliptic_curves.value().size()];
      for (auto const &curve : config.tls_elliptic_curves.value()) {
        [tlsEllipticCurves addObject:[NSString stringForStdString:curve]];
      }
      _tlsEllipticCurves = tlsEllipticCurves;
    }
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCSslConfig:\n%d\n%d\n%d\n%d\n%@\n%@\n%@",
                                    _enableOcspStapling,
                                    _enableSignedCertTimestamp,
                                    _enableTlsChannelId,
                                    _enableGrease,
                                    _maxSslVersion,
                                    _tlsAlpnProtocols,
                                    _tlsEllipticCurves];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::SSLConfig)nativeConfig {
  __block webrtc::PeerConnectionInterface::SSLConfig sslConfig;

  sslConfig.enable_ocsp_stapling = _enableOcspStapling;
  sslConfig.enable_signed_cert_timestamp = _enableSignedCertTimestamp;
  sslConfig.enable_tls_channel_id = _enableTlsChannelId;
  sslConfig.enable_grease = _enableGrease;

  if (_maxSslVersion != nil) {
    sslConfig.max_ssl_version = absl::optional<int>(_maxSslVersion.intValue);
  }

  if (_tlsAlpnProtocols != nil) {
    __block std::vector<std::string> alpn_protocols;
    [_tlsAlpnProtocols enumerateObjectsUsingBlock:^(NSString *proto, NSUInteger idx, BOOL *stop) {
      alpn_protocols.push_back(proto.stdString);
    }];
    sslConfig.tls_alpn_protocols = absl::optional<std::vector<std::string>>(alpn_protocols);
  }

  if (_tlsEllipticCurves != nil) {
    __block std::vector<std::string> elliptic_curves;
    [_tlsEllipticCurves enumerateObjectsUsingBlock:^(NSString *curve, NSUInteger idx, BOOL *stop) {
      elliptic_curves.push_back(curve.stdString);
    }];
    sslConfig.tls_elliptic_curves = absl::optional<std::vector<std::string>>(elliptic_curves);
  }

  return sslConfig;
}

@end
