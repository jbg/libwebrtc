/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactoryOptions+Private.h"

#include "rtc_base/network_constants.h"

@implementation RTCPeerConnectionFactoryOptions

@synthesize disableEncryption = _disableEncryption;
@synthesize disableNetworkMonitor = _disableNetworkMonitor;
@synthesize ignoreLoopbackNetworkAdapter = _ignoreLoopbackNetworkAdapter;
@synthesize ignoreVPNNetworkAdapter = _ignoreVPNNetworkAdapter;
@synthesize ignoreCellularNetworkAdapter = _ignoreCellularNetworkAdapter;
@synthesize ignoreWiFiNetworkAdapter = _ignoreWiFiNetworkAdapter;
@synthesize ignoreEthernetNetworkAdapter = _ignoreEthernetNetworkAdapter;

- (instancetype)init {
  return [super init];
}

- (webrtc::PeerConnectionFactoryInterface::Options)nativeOptions {
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = self.disableEncryption;
  options.disable_network_monitor = self.disableNetworkMonitor;

  if (self.ignoreLoopbackNetworkAdapter) {
    options.network_ignore_mask |= rtc::ADAPTER_TYPE_LOOPBACK;
  } else {
    options.network_ignore_mask &= ~rtc::ADAPTER_TYPE_LOOPBACK;
  }

  if (self.ignoreVPNNetworkAdapter) {
    options.network_ignore_mask |= rtc::ADAPTER_TYPE_VPN;
  } else {
    options.network_ignore_mask &= ~rtc::ADAPTER_TYPE_VPN;
  }

  if (self.ignoreCellularNetworkAdapter) {
    options.network_ignore_mask |= rtc::ADAPTER_TYPE_CELLULAR;
  } else {
    options.network_ignore_mask &= ~rtc::ADAPTER_TYPE_CELLULAR;
  }

  if (self.ignoreWiFiNetworkAdapter) {
    options.network_ignore_mask |= rtc::ADAPTER_TYPE_WIFI;
  } else {
    options.network_ignore_mask &= ~rtc::ADAPTER_TYPE_WIFI;
  }

  if (self.ignoreEthernetNetworkAdapter) {
    options.network_ignore_mask |= rtc::ADAPTER_TYPE_ETHERNET;
  } else {
    options.network_ignore_mask &= ~rtc::ADAPTER_TYPE_ETHERNET;
  }

  return options;
}

@end
