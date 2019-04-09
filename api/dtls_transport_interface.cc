/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/dtls_transport_interface.h"

namespace webrtc {

DtlsTransportInformation::DtlsTransportInformation(DtlsTransportState state)
    : state_(state) {}

DtlsTransportInformation::DtlsTransportInformation(DtlsTransportState state,
                                                   int ssl_cipher_suite)
    : state_(state), ssl_cipher_suite_(ssl_cipher_suite) {}

}  // namespace webrtc
