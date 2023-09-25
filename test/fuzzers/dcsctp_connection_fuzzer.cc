/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/fuzzers/dcsctp_fuzzers.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "rtc_base/logging.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  dcsctp::dcsctp_fuzzers::FuzzedSocket socket_a("A");
  dcsctp::dcsctp_fuzzers::FuzzedSocket socket_z("Z");

  dcsctp::dcsctp_fuzzers::FuzzConnection(
      socket_a, socket_z, rtc::ArrayView<const uint8_t>(data, size));
}
}  // namespace webrtc
