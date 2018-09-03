/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/cryptoparams.h"

namespace cricket {

CryptoParams::CryptoParams() : tag(0) {}

CryptoParams::CryptoParams(int t,
             const std::string& cs,
             const std::string& kp,
             const std::string& sp)
    : tag(t), cipher_suite(cs), key_params(kp), session_params(sp) {}

CryptoParams::CryptoParams(const CryptoParams& rhs) = default;

}  // namespace cricket
