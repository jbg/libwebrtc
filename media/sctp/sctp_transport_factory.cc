/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/sctp_transport_factory.h"

#include "media/sctp/sctp_transport.h"

namespace cricket {

SctpTransportFactory::SctpTransportFactory(rtc::Thread* network_thread,
                                           rtc::Thread* usrsctp_thread)
    : network_thread_(network_thread), usrsctp_thread_(usrsctp_thread) {}

std::unique_ptr<SctpTransportInternal>
SctpTransportFactory::CreateSctpTransport(
    rtc::PacketTransportInternal* transport) {
  return std::unique_ptr<SctpTransportInternal>(
      new SctpTransport(network_thread_, usrsctp_thread_, transport));
}

}  // namespace cricket
