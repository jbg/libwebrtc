/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/p2phelper.h"

#include "p2p/base/port.h"  // For TCP_PROTOCOL_NAME and SSLTCP_PROTOCOL_NAME

namespace cricket {

int GetIpOverhead(int addr_family) {
  switch (addr_family) {
    case AF_INET:  // IPv4
      return 20;
    case AF_INET6:  // IPv6
      return 40;
    default:
      RTC_NOTREACHED();
  }
}

int GetProtocolOverhead(const std::string& protocol) {
  if (protocol == TCP_PROTOCOL_NAME || protocol == SSLTCP_PROTOCOL_NAME) {
    return 20;
  }
  return 8;
}

}  // namespace cricket
