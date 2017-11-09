/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef P2P_BASE_P2PHELPER_H_
#define P2P_BASE_P2PHELPER_H_

#include <cstdlib>
#include <string>

// This head contains helper methods used in different types of transports.
namespace cricket {

int GetIpOverhead(int addr_family);

int GetProtocolOverhead(const std::string& protocol);

}  // namespace cricket

#endif  // P2P_BASE_P2PHELPER_H_
