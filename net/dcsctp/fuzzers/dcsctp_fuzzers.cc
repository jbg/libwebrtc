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

#include <string>
#include <utility>
#include <vector>

#include "net/dcsctp/common/math.h"
#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"
#include "net/dcsctp/packet/chunk/cookie_echo_chunk.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/shutdown_chunk.h"
#include "net/dcsctp/packet/error_cause/protocol_violation_cause.h"
#include "net/dcsctp/packet/error_cause/user_initiated_abort_cause.h"
#include "net/dcsctp/packet/parameter/forward_tsn_supported_parameter.h"
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/state_cookie_parameter.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "net/dcsctp/socket/state_cookie.h"
#include "rtc_base/crc32.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {
namespace dcsctp_fuzzers {
namespace {
static constexpr int kRandomValue = FuzzerCallbacks::kRandomValue;
static constexpr size_t kMinInputLength = 5;
static constexpr size_t kMaxInputLength = 1024;
constexpr size_t kMaxMessageSize = 3012;

struct DataPayloadHeader {
  uint32_t message_id;
  uint32_t stream_id;
  uint32_t size;
  uint32_t crc;
};

// A starting state for the socket, when fuzzing.
enum class StartingState : int {
  kConnectNotCalled,
  // When socket initiating Connect
  kConnectCalled,
  kReceivedInitAck,
  kReceivedCookieAck,
  // When socket initiating Shutdown
  kShutdownCalled,
  kReceivedShutdownAck,
  // When peer socket initiated Connect
  kReceivedInit,
  kReceivedCookieEcho,
  // When peer initiated Shutdown
  kReceivedShutdown,
  kReceivedShutdownComplete,
  kNumberOfStates,
};

// State about the current fuzzing iteration
class FuzzState {
 public:
  explicit FuzzState(rtc::ArrayView<const uint8_t> data) : data_(data) {}

  uint8_t GetByte() {
    uint8_t value = 0;
    if (offset_ < data_.size()) {
      value = data_[offset_];
      ++offset_;
    }
    return value;
  }

  TSN GetNextTSN() { return TSN(tsn_++); }
  MID GetNextMID() { return MID(mid_++); }

  bool empty() const { return offset_ >= data_.size(); }

 private:
  uint32_t tsn_ = kRandomValue;
  uint32_t mid_ = 0;
  rtc::ArrayView<const uint8_t> data_;
  size_t offset_ = 0;
};

void SetSocketState(FuzzedSocket& a, FuzzedSocket& z, StartingState state) {
  switch (state) {
    case StartingState::kConnectNotCalled:
      return;
    case StartingState::kConnectCalled:
      a.socket.Connect();
      return;
    case StartingState::kReceivedInitAck:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      return;
    case StartingState::kReceivedCookieAck:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // COOKIE_ECHO
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ACK
      return;
    case StartingState::kShutdownCalled:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // COOKIE_ECHO
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ACK
      a.socket.Shutdown();
      return;
    case StartingState::kReceivedShutdownAck:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // COOKIE_ECHO
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ACK
      a.socket.Shutdown();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // SHUTDOWN
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // SHUTDOWN_ACK
      return;
    case StartingState::kReceivedInit:
      z.socket.Connect();
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT
      return;
    case StartingState::kReceivedCookieEcho:
      z.socket.Connect();
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT_ACK
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ECHO
      return;
    case StartingState::kReceivedShutdown:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // COOKIE_ECHO
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ACK
      z.socket.Shutdown();
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // SHUTDOWN
      return;
    case StartingState::kReceivedShutdownComplete:
      a.socket.Connect();
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // INIT
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // INIT_ACK
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // COOKIE_ECHO
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // COOKIE_ACK
      z.socket.Shutdown();
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // SHUTDOWN
      z.socket.ReceivePacket(a.cb.ConsumeSentPacket());  // SHUTDOWN_ACK
      a.socket.ReceivePacket(z.cb.ConsumeSentPacket());  // SHUTDOWN_COMPLETE
      return;
    case StartingState::kNumberOfStates:
      RTC_CHECK(false);
      return;
  }
}

void MakeDataChunk(FuzzState& state, SctpPacket::Builder& b) {
  DataChunk::Options options;
  options.is_unordered = IsUnordered(state.GetByte() != 0);
  options.is_beginning = Data::IsBeginning(state.GetByte() != 0);
  options.is_end = Data::IsEnd(state.GetByte() != 0);
  b.Add(DataChunk(state.GetNextTSN(), StreamID(state.GetByte()),
                  SSN(state.GetByte()), PPID(53), std::vector<uint8_t>(10),
                  options));
}

void MakeInitChunk(FuzzState& state, SctpPacket::Builder& b) {
  Parameters::Builder builder;
  builder.Add(ForwardTsnSupportedParameter());

  b.Add(InitChunk(VerificationTag(kRandomValue), 10000, 1000, 1000,
                  TSN(kRandomValue), builder.Build()));
}

void MakeInitAckChunk(FuzzState& state, SctpPacket::Builder& b) {
  Parameters::Builder builder;
  builder.Add(ForwardTsnSupportedParameter());

  uint8_t state_cookie[] = {1, 2, 3, 4, 5};
  Parameters::Builder params_builder =
      Parameters::Builder().Add(StateCookieParameter(state_cookie));

  b.Add(InitAckChunk(VerificationTag(kRandomValue), 10000, 1000, 1000,
                     TSN(kRandomValue), builder.Build()));
}

void MakeSackChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<SackChunk::GapAckBlock> gap_ack_blocks;
  uint16_t last_end = 0;
  while (gap_ack_blocks.size() < 20) {
    uint8_t delta_start = state.GetByte();
    if (delta_start < 0x80) {
      break;
    }
    uint8_t delta_end = state.GetByte();

    uint16_t start = last_end + delta_start;
    uint16_t end = start + delta_end;
    last_end = end;
    gap_ack_blocks.emplace_back(start, end);
  }

  TSN cum_ack_tsn(kRandomValue + state.GetByte());
  b.Add(SackChunk(cum_ack_tsn, 10000, std::move(gap_ack_blocks), {}));
}

void MakeHeartbeatRequestChunk(FuzzState& state, SctpPacket::Builder& b) {
  uint8_t info[] = {1, 2, 3, 4, 5};
  b.Add(HeartbeatRequestChunk(
      Parameters::Builder().Add(HeartbeatInfoParameter(info)).Build()));
}

void MakeHeartbeatAckChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<uint8_t> info(8);
  b.Add(HeartbeatRequestChunk(
      Parameters::Builder().Add(HeartbeatInfoParameter(info)).Build()));
}

void MakeAbortChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(AbortChunk(
      /*filled_in_verification_tag=*/true,
      Parameters::Builder().Add(UserInitiatedAbortCause("Fuzzing")).Build()));
}

void MakeErrorChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(ErrorChunk(
      Parameters::Builder().Add(ProtocolViolationCause("Fuzzing")).Build()));
}

void MakeCookieEchoChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<uint8_t> cookie(StateCookie::kCookieSize);
  b.Add(CookieEchoChunk(cookie));
}

void MakeCookieAckChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(CookieAckChunk());
}

void MakeShutdownChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(ShutdownChunk(state.GetNextTSN()));
}

void MakeShutdownAckChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(ShutdownAckChunk());
}

void MakeShutdownCompleteChunk(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(ShutdownCompleteChunk(false));
}

void MakeReConfigChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<StreamID> streams = {StreamID(state.GetByte())};
  Parameters::Builder params_builder =
      Parameters::Builder().Add(OutgoingSSNResetRequestParameter(
          ReconfigRequestSN(kRandomValue), ReconfigRequestSN(kRandomValue),
          state.GetNextTSN(), streams));
  b.Add(ReConfigChunk(params_builder.Build()));
}

void MakeForwardTsnChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<ForwardTsnChunk::SkippedStream> skipped_streams;
  for (;;) {
    uint8_t stream = state.GetByte();
    if (skipped_streams.size() > 20 || stream < 0x80) {
      break;
    }
    skipped_streams.emplace_back(StreamID(stream), SSN(state.GetByte()));
  }
  b.Add(ForwardTsnChunk(state.GetNextTSN(), std::move(skipped_streams)));
}

void MakeIDataChunk(FuzzState& state, SctpPacket::Builder& b) {
  DataChunk::Options options;
  options.is_unordered = IsUnordered(state.GetByte() != 0);
  options.is_beginning = Data::IsBeginning(state.GetByte() != 0);
  options.is_end = Data::IsEnd(state.GetByte() != 0);
  b.Add(IDataChunk(state.GetNextTSN(), StreamID(state.GetByte()),
                   state.GetNextMID(), PPID(53), FSN(0),
                   std::vector<uint8_t>(10), options));
}

void MakeIForwardTsnChunk(FuzzState& state, SctpPacket::Builder& b) {
  std::vector<ForwardTsnChunk::SkippedStream> skipped_streams;
  for (;;) {
    uint8_t stream = state.GetByte();
    if (skipped_streams.size() > 20 || stream < 0x80) {
      break;
    }
    skipped_streams.emplace_back(StreamID(stream), SSN(state.GetByte()));
  }
  b.Add(IForwardTsnChunk(state.GetNextTSN(), std::move(skipped_streams)));
}

class RandomFuzzedChunk : public Chunk {
 public:
  explicit RandomFuzzedChunk(FuzzState& state) : state_(state) {}

  void SerializeTo(std::vector<uint8_t>& out) const override {
    size_t bytes = state_.GetByte();
    for (size_t i = 0; i < bytes; ++i) {
      out.push_back(state_.GetByte());
    }
  }

  std::string ToString() const override { return std::string("RANDOM_FUZZED"); }

 private:
  FuzzState& state_;
};

void MakeChunkWithRandomContent(FuzzState& state, SctpPacket::Builder& b) {
  b.Add(RandomFuzzedChunk(state));
}

std::vector<uint8_t> GeneratePacket(FuzzState& state) {
  DcSctpOptions options;
  // Setting a fixed limit to not be dependent on the defaults, which may
  // change.
  options.mtu = 2048;
  SctpPacket::Builder builder(VerificationTag(kRandomValue), options);

  // The largest expected serialized chunk, as created by fuzzers.
  static constexpr size_t kMaxChunkSize = 256;

  for (int i = 0; i < 5 && builder.bytes_remaining() > kMaxChunkSize; ++i) {
    switch (state.GetByte()) {
      case 1:
        MakeDataChunk(state, builder);
        break;
      case 2:
        MakeInitChunk(state, builder);
        break;
      case 3:
        MakeInitAckChunk(state, builder);
        break;
      case 4:
        MakeSackChunk(state, builder);
        break;
      case 5:
        MakeHeartbeatRequestChunk(state, builder);
        break;
      case 6:
        MakeHeartbeatAckChunk(state, builder);
        break;
      case 7:
        MakeAbortChunk(state, builder);
        break;
      case 8:
        MakeErrorChunk(state, builder);
        break;
      case 9:
        MakeCookieEchoChunk(state, builder);
        break;
      case 10:
        MakeCookieAckChunk(state, builder);
        break;
      case 11:
        MakeShutdownChunk(state, builder);
        break;
      case 12:
        MakeShutdownAckChunk(state, builder);
        break;
      case 13:
        MakeShutdownCompleteChunk(state, builder);
        break;
      case 14:
        MakeReConfigChunk(state, builder);
        break;
      case 15:
        MakeForwardTsnChunk(state, builder);
        break;
      case 16:
        MakeIDataChunk(state, builder);
        break;
      case 17:
        MakeIForwardTsnChunk(state, builder);
        break;
      case 18:
        MakeChunkWithRandomContent(state, builder);
        break;
      default:
        break;
    }
  }
  std::vector<uint8_t> packet = builder.Build();
  return packet;
}
}  // namespace

void FuzzerCallbacks::OnMessageReceived(DcSctpMessage message) {
  size_t size = message.payload().size();
  RTC_CHECK_EQ(size % 4, 0);
  RTC_CHECK_GE(size, sizeof(DataPayloadHeader));
  RTC_CHECK_LE(size, kMaxMessageSize);

  const DataPayloadHeader* hdr =
      reinterpret_cast<const DataPayloadHeader*>(message.payload().data());
  RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Received message on sid="
                    << *message.stream_id()
                    << ", message_id=" << hdr->message_id
                    << ", size=" << message.payload().size()
                    << ", hdr_size=" << hdr->size;

  auto [unused, inserted] = received_message_ids_.insert(hdr->message_id);
  RTC_CHECK(inserted);
  RTC_CHECK_EQ(hdr->stream_id, message.stream_id().value());
  RTC_CHECK_EQ(hdr->size, size);
  uint32_t crc = rtc::ComputeCrc32(message.payload().data() + sizeof(*hdr),
                                   size - sizeof(*hdr));
  RTC_CHECK_EQ(hdr->crc, crc);
}

void FuzzSocket(FuzzedSocket& a, rtc::ArrayView<const uint8_t> data) {
  if (data.size() < kMinInputLength || data.size() > kMaxInputLength) {
    return;
  }
  if (data[0] >= static_cast<int>(StartingState::kNumberOfStates)) {
    return;
  }

  // Set the socket in a specified valid starting state
  // We'll use another temporary peer socket for the establishment.
  FuzzedSocket z("Z");
  SetSocketState(a, z, static_cast<StartingState>(data[0]));

  FuzzState state(data.subview(1));

  while (!state.empty()) {
    switch (state.GetByte()) {
      case 1:
        // Generate a valid SCTP packet (based on fuzz data) and "receive it".
        a.socket.ReceivePacket(GeneratePacket(state));
        break;
      case 2:
        a.socket.Connect();
        break;
      case 3:
        a.socket.Shutdown();
        break;
      case 4:
        a.socket.Close();
        break;
      case 5: {
        StreamID streams[] = {StreamID(state.GetByte())};
        a.socket.ResetStreams(streams);
      } break;
      case 6: {
        uint8_t flags = state.GetByte();
        SendOptions options;
        options.unordered = IsUnordered(flags & 0x01);
        options.max_retransmissions =
            (flags & 0x02) != 0 ? absl::make_optional(0) : absl::nullopt;
        options.lifecycle_id = LifecycleId(42);
        size_t payload_exponent = (flags >> 2) % 16;
        size_t payload_size = static_cast<size_t>(1) << payload_exponent;
        a.socket.Send(DcSctpMessage(StreamID(state.GetByte()), PPID(53),
                                    std::vector<uint8_t>(payload_size)),
                      options);
        break;
      }
      case 7: {
        // Expire the next timeout/timer.
        TimeMs ts = a.cb.PeekNextExpiryTime();
        if (ts != TimeMs::InfiniteFuture()) {
          absl::optional<TimeoutID> timeout_id = a.cb.AdvanceTimeTowards(ts);
          if (timeout_id.has_value()) {
            a.socket.HandleTimeout(*timeout_id);
          }
        }
        break;
      }
      default:
        break;
    }
  }
}

void ExchangeMessages(FuzzedSocket& a, FuzzedSocket& z, int max_count = 1000) {
  bool delivered_packet = false;
  do {
    delivered_packet = false;
    std::vector<uint8_t> packet_from_a = a.cb.ConsumeSentPacket();
    if (!packet_from_a.empty()) {
      delivered_packet = true;
      z.socket.ReceivePacket(std::move(packet_from_a));
    }
    std::vector<uint8_t> packet_from_z = z.cb.ConsumeSentPacket();
    if (!packet_from_z.empty()) {
      delivered_packet = true;
      a.socket.ReceivePacket(std::move(packet_from_z));
    }
  } while (--max_count > 0 && delivered_packet);
}

std::vector<FuzzCommand> MakeFuzzCommands(rtc::ArrayView<const uint8_t> data) {
  std::vector<FuzzCommand> commands;
  if (data.size() < kMinInputLength || data.size() > kMaxInputLength) {
    return commands;
  }
  FuzzState state(data);

  while (!state.empty()) {
    uint8_t cmd = state.GetByte();
    bool actor_is_a = (cmd & 0x01) == 0;
    cmd = cmd >> 1;
    if (cmd == 0) {
      commands.push_back(FuzzCommandAdvanceTime{});
    } else if (cmd >= 1 && cmd < 5) {
      commands.push_back(
          FuzzCommandReceivePackets{.a_to_z = actor_is_a, .count = cmd});
    } else if (cmd == 5) {
      commands.push_back(FuzzCommandDropPacket{.socket_is_a = actor_is_a});
    } else if (cmd >= 6 && cmd < 22) {
      uint8_t flags = cmd - 6;
      commands.push_back(FuzzCommandSendMessage{
          .socket_is_a = actor_is_a,
          .stream_id = (flags & 0x8) == 0 ? 1 : 2,
          .unordered = (flags & 0x4) == 0,
          .max_retransmissions = (flags & 0x02) == 0 ? -1 : 0,
          .message_size = (flags & 0x1) == 0 ? 100 : kMaxMessageSize,
      });
    } else if (cmd >= 22 && cmd < 26) {
      uint8_t flags = cmd - 22;
      commands.push_back(
          FuzzCommandResetStream{.socket_is_a = actor_is_a,
                                 .reset_1 = (flags & 0x02) != 0,
                                 .reset_2 = (flags & 0x01) != 0});
    } else if (cmd >= 26 && cmd < 36) {
      commands.push_back((FuzzCommandRetransmitPacket{
          .a_to_z = actor_is_a, .lookback = static_cast<size_t>(cmd - 26)}));
    }
  }

  return commands;
}
struct FuzzCommandPrinter {
  rtc::StringBuilder sb;

  void operator()(FuzzCommandAdvanceTime cmd) {
    sb << "FuzzCommandAdvanceTime{},\n";
  }
  void operator()(FuzzCommandReceivePackets cmd) {
    sb << "FuzzCommandReceivePackets{.a_to_z=" << cmd.a_to_z
       << ", .count=" << cmd.count << "},\n";
  }
  void operator()(FuzzCommandDropPacket cmd) {
    sb << "FuzzCommandDropPacket{.socket_is_a=" << cmd.socket_is_a << "},\n";
  }
  void operator()(FuzzCommandRetransmitPacket cmd) {
    sb << "FuzzCommandRetransmitPacket{.a_to_z=" << cmd.a_to_z
       << ", .lookback=" << cmd.lookback << "},\n";
  }
  void operator()(FuzzCommandSendMessage cmd) {
    sb << "FuzzCommandSendMessage{.socket_is_a=" << cmd.socket_is_a
       << ", .stream_id=" << cmd.stream_id << ", .unordered=" << cmd.unordered
       << ", .max_retransmissions=" << cmd.max_retransmissions
       << ", .message_size=" << cmd.message_size << "},\n";
  }
  void operator()(FuzzCommandResetStream cmd) {
    sb << "FuzzCommandResetStream{.socket_is_a=" << cmd.socket_is_a
       << ", .reset_1=" << cmd.reset_1 << ", .reset_2=" << cmd.reset_2
       << "},\n";
  }
};

std::string PrintFuzzCommands(rtc::ArrayView<const FuzzCommand> commands) {
  FuzzCommandPrinter fcp{};
  for (const auto& command : commands) {
    std::visit(fcp, command);
  }
  return fcp.sb.Release();
}

std::string PrintFuzzCommand(FuzzCommand cmd) {
  FuzzCommandPrinter fcp{};
  std::visit(fcp, cmd);
  return fcp.sb.Release();
}

void ExecuteFuzzCommands(FuzzedSocket& a,
                         FuzzedSocket& z,
                         rtc::ArrayView<const FuzzCommand> commands) {
  struct FuzzCommandExecutor {
    FuzzedSocket& a;
    FuzzedSocket& z;
    webrtc::Random& random;
    bool command_was_useful = false;

    void operator()(FuzzCommandAdvanceTime cmd) {
      // Move time to next interesting event.
      TimeMs a_next_time = a.cb.PeekNextExpiryTime();
      TimeMs z_next_time = z.cb.PeekNextExpiryTime();
      TimeMs next_time = std::min(a_next_time, z_next_time);
      if (next_time != TimeMs::InfiniteFuture()) {
        command_was_useful = true;
        RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Advancing time " << *next_time
                          << " ms";
        RTC_DCHECK(next_time >= a.cb.TimeMillis());
        for (;;) {
          absl::optional<TimeoutID> timeout_a =
              a.cb.AdvanceTimeTowards(next_time);
          if (timeout_a.has_value()) {
            a.socket.HandleTimeout(*timeout_a);
          }
          absl::optional<TimeoutID> timeout_z =
              z.cb.AdvanceTimeTowards(next_time);
          if (timeout_z.has_value()) {
            z.socket.HandleTimeout(*timeout_z);
          }
          if (!timeout_a.has_value() && !timeout_z.has_value()) {
            break;
          }
        }
        RTC_DCHECK(a.cb.TimeMillis() == next_time);
        RTC_DCHECK(z.cb.TimeMillis() == next_time);
      }
    }
    void operator()(FuzzCommandReceivePackets cmd) {
      FuzzedSocket& from = (cmd.a_to_z) ? a : z;
      FuzzedSocket& to = (cmd.a_to_z) ? z : a;
      for (size_t i = 0; i < cmd.count; ++i) {
        std::vector<uint8_t> packet = from.cb.ConsumeSentPacket();
        if (!packet.empty()) {
          command_was_useful = true;
          RTC_DLOG(LS_INFO)
              << "SCTP_FUZZ: Received packet on " << to.cb.GetName();
          to.socket.ReceivePacket(std::move(packet));
        }
      }
    }
    void operator()(FuzzCommandDropPacket cmd) {
      FuzzedSocket& socket = (cmd.socket_is_a) ? a : z;
      std::vector<uint8_t> dropped_packet = socket.cb.ConsumeSentPacket();
      if (!dropped_packet.empty()) {
        command_was_useful = true;
        RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Dropped packet on "
                          << socket.cb.GetName();
      }
    }
    void operator()(FuzzCommandRetransmitPacket cmd) {
      FuzzedSocket& from = (cmd.a_to_z) ? a : z;
      FuzzedSocket& to = (cmd.a_to_z) ? z : a;
      std::vector<uint8_t> packet = from.cb.GetPacketFromHistory(cmd.lookback);
      if (!packet.empty()) {
        command_was_useful = true;
        RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Re-receiving packet, lookback="
                          << cmd.lookback << " on " << to.cb.GetName();
        to.socket.ReceivePacket(std::move(packet));
      }
    }
    void operator()(FuzzCommandSendMessage cmd) {
      command_was_useful = true;
      FuzzedSocket& socket = (cmd.socket_is_a) ? a : z;
      SendOptions options;
      options.unordered = IsUnordered(cmd.unordered);
      options.max_retransmissions =
          cmd.max_retransmissions < 0
              ? absl::nullopt
              : absl::make_optional(cmd.max_retransmissions);
      std::vector<uint8_t> payload(cmd.message_size);
      uint32_t* data = reinterpret_cast<uint32_t*>(payload.data());
      for (size_t i = 0; i < cmd.message_size / 4; ++i) {
        data[i] = random.Rand<uint32_t>();
      }

      DataPayloadHeader* hdr =
          reinterpret_cast<DataPayloadHeader*>(payload.data());
      hdr->message_id = socket.cb.GetNextMessageId();
      hdr->stream_id = cmd.stream_id;
      hdr->size = cmd.message_size;
      hdr->crc = rtc::ComputeCrc32(payload.data() + sizeof(*hdr),
                                   cmd.message_size - sizeof(*hdr));

      RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Sending message on sid=" << cmd.stream_id
                        << ", message_id=" << hdr->message_id
                        << ", size=" << hdr->size;

      socket.socket.Send(
          DcSctpMessage(StreamID(cmd.stream_id), PPID(53), std::move(payload)),
          options);
    }
    void operator()(FuzzCommandResetStream cmd) {
      command_was_useful = true;
      FuzzedSocket& socket = (cmd.socket_is_a) ? a : z;
      std::vector<StreamID> streams;
      if (cmd.reset_1) {
        streams.push_back((StreamID(1)));
      }
      if (cmd.reset_2) {
        streams.push_back((StreamID(2)));
      }
      socket.socket.ResetStreams(streams);
    }
  };
  SetSocketState(a, z, StartingState::kReceivedCookieAck);

  webrtc::Random random(42);

  for (const auto& command : commands) {
    if (a.cb.IsAborted() || z.cb.IsAborted()) {
      return;
    }
    RTC_DLOG(LS_INFO) << "SCTP_FUZZ: " << PrintFuzzCommand(command);
    FuzzCommandExecutor executor{a, z, random};
    std::visit(executor, command);
    if (!executor.command_was_useful) {
      RTC_DLOG(LS_INFO) << "SCTP_FUZZ: Previous command wasn't useful - abort";
      //return;
    }
  }

  // Deliver all remaining messages.
  ExchangeMessages(a, z);
}

void FuzzConnection(FuzzedSocket& a,
                    FuzzedSocket& z,
                    rtc::ArrayView<const uint8_t> data) {
  std::vector<FuzzCommand> commands = MakeFuzzCommands(data);
  if (commands.empty()) {
    return;
  }
  ExecuteFuzzCommands(a, z, commands);
}

}  // namespace dcsctp_fuzzers
}  // namespace dcsctp
