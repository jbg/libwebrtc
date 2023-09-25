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

#include "absl/flags/flag.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/text_pcap_packet_observer.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"

ABSL_FLAG(bool, dcsctp_capture_packets2, false, "Print packet capture.");

namespace dcsctp {
namespace dcsctp_fuzzers {
namespace {

std::unique_ptr<PacketObserver> GetPacketObserver(absl::string_view name) {
  if (absl::GetFlag(FLAGS_dcsctp_capture_packets2)) {
    return std::make_unique<TextPcapPacketObserver>(name);
  }
  return nullptr;
}

// This is a testbed where fuzzed data that cause issues can be evaluated and
// crashes reproduced. Use `xxd -i ./crash-abc` to generate `data` below.

// for dcsctp_socket_fuzzer
TEST(DcsctpFuzzersTest, CanFuzzSocket) {
  uint8_t data[] = {0x07, 0x09, 0x00, 0x01, 0x11, 0xff, 0xff};

  FuzzedSocket socket("A");

  FuzzSocket(socket, data);
}

// for dcsctp_connection_fuzzer
TEST(DcsctpFuzzersTest, CanPrintFuzzConnection) {
  uint8_t data[] = {0x27, 0x2a, 0x32, 0x04, 0x27, 0x18, 0x09,
                    0x04, 0x09, 0x00, 0x2a, 0x32, 0x24};
  RTC_LOG(LS_INFO) << "\n" << PrintFuzzCommands(MakeFuzzCommands(data));
}

// for dcsctp_connection_fuzzer
TEST(DcsctpFuzzersTest, CanFuzzConnectionFromBinary) {
  uint8_t data[] = {0x27, 0x2a, 0x32, 0x04, 0x27, 0x18, 0x09,
                    0x04, 0x09, 0x00, 0x2a, 0x32, 0x24};
  auto commands = MakeFuzzCommands(data);

  FuzzedSocket a("A", GetPacketObserver("A"));
  FuzzedSocket z("Z", GetPacketObserver("Z"));

  ExecuteFuzzCommands(a, z, commands);
}

// for dcsctp_connection_fuzzer
TEST(DcsctpFuzzersTest, CanFuzzConnectionFromCommands) {
  std::vector<FuzzCommand> commands = {
      FuzzCommandSendMessage{.socket_is_a = 1,
                             .stream_id = 2,
                             .unordered = 0,
                             .max_retransmissions = 0,
                             .message_size = 2000},
      FuzzCommandResetStream{.socket_is_a = 1, .reset_1 = 0, .reset_2 = 1},
      FuzzCommandReceivePackets{.a_to_z = 1, .count = 1},
      FuzzCommandSendMessage{.socket_is_a = 1,
                             .stream_id = 1,
                             .unordered = 1,
                             .max_retransmissions = 0,
                             .message_size = 100},
      FuzzCommandReceivePackets{.a_to_z = 1, .count = 2},
      FuzzCommandReceivePackets{.a_to_z = 0, .count = 2},
      FuzzCommandAdvanceTime{},
      FuzzCommandSendMessage{.socket_is_a = 1,
                             .stream_id = 2,
                             .unordered = 0,
                             .max_retransmissions = -1,
                             .message_size = 100},
      FuzzCommandResetStream{.socket_is_a = 1, .reset_1 = 0, .reset_2 = 1},
      FuzzCommandSendMessage{.socket_is_a = 1,
                             .stream_id = 2,
                             .unordered = 0,
                             .max_retransmissions = -1,
                             .message_size = 100},
  };

  FuzzedSocket a("A", GetPacketObserver("A"));
  FuzzedSocket z("Z", GetPacketObserver("Z"));

  ExecuteFuzzCommands(a, z, commands);
}

}  // namespace
}  // namespace dcsctp_fuzzers
}  // namespace dcsctp
