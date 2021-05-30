/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/chunk/cookie_echo_chunk.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/chunk/error_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_ack_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_request_chunk.h"
#include "net/dcsctp/packet/chunk/idata_chunk.h"
#include "net/dcsctp/packet/chunk/init_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/chunk/shutdown_chunk.h"
#include "net/dcsctp/packet/error_cause/error_cause.h"
#include "net/dcsctp/packet/error_cause/unrecognized_chunk_type_cause.h"
#include "net/dcsctp/packet/parameter/heartbeat_info_parameter.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/packet_observer.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/rx/reassembly_queue.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "net/dcsctp/timer/task_queue_timeout.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::SizeIs;

constexpr size_t kSmallPayloadSize = 10;
constexpr size_t kLargePayloadSize = 10000;
constexpr size_t kHugePayloadSize = 262144;
constexpr size_t kBufferedAmountLowThreshold = kLargePayloadSize * 2;

const rtc::SocketAddress kInitialAddr(rtc::IPAddress(INADDR_ANY), 0);

inline int GetUniqueSeed() {
  static int seed = 0;
  return ++seed;
}

DcSctpOptions MakeOptionsForTest() {
  DcSctpOptions options;

  // By disabling the heartbeat interval, there will no timers at all running
  // when the socket is idle, which makes it easy to just continue the test
  // until there are no more scheduled tasks. Note that it _will_ run for longer
  // than necessary as timers aren't cancelled when they are stopped (as that's
  // not supported), but it's still simulated time and passes quickly.
  options.heartbeat_interval = DurationMs(0);

  // options.slow_start_tcp_style = true;
  return options;
}

// Print outs all sent and received packets to the logs, at LS_VERBOSE severity.
class TextPcapPacketObserver : public dcsctp::PacketObserver {
 public:
  explicit TextPcapPacketObserver(absl::string_view name) : name_(name) {}

  void OnSentPacket(dcsctp::TimeMs now, rtc::ArrayView<const uint8_t> payload) {
    PrintPacket("\nO ", now, payload);
  }

  void OnReceivedPacket(dcsctp::TimeMs now,
                        rtc::ArrayView<const uint8_t> payload) {
    PrintPacket("\nI ", now, payload);
  }

 private:
  void PrintPacket(absl::string_view prefix,
                   dcsctp::TimeMs now,
                   rtc::ArrayView<const uint8_t> payload) {
    rtc::StringBuilder s;
    s << prefix;
    int64_t remaining = *now % (24 * 60 * 60 * 1000);
    int hours = remaining / (60 * 60 * 1000);
    remaining = remaining % (60 * 60 * 1000);
    int minutes = remaining / (60 * 1000);
    remaining = remaining % (60 * 1000);
    int seconds = remaining / 1000;
    int ms = remaining % 1000;
    s.AppendFormat("%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
    s << " 0000";
    for (uint8_t byte : payload) {
      s.AppendFormat(" %02x", byte);
    }
    s << " # SCTP_PACKET " << name_;
    RTC_LOG(LS_VERBOSE) << s.str();
  }

  const std::string name_;
};

std::unique_ptr<PacketObserver> MaybeMakePacketObserver(
    absl::string_view name) {
  if (RTC_LOG_CHECK_LEVEL(LS_VERBOSE)) {
    return std::make_unique<TextPcapPacketObserver>(name);
  }
  return nullptr;
}

// When doing throughput tests, knowing what each actor should do.
enum class ActorMode {
  kAtRest,
  kThroughputSender,
  kThroughputReceiver,
  kLimitedRetransmissionSender,
};

enum class MessageId : uint32_t {
  kPrintBandwidth = 1,
};

// Sends at a constant rate but with random packet sizes.
class SctpActor : public rtc::MessageHandlerAutoCleanup,
                  public DcSctpSocketCallbacks,
                  public sigslot::has_slots<> {
 public:
  SctpActor(absl::string_view name,
            rtc::ClockInterface& clock,
            rtc::Thread* thread,
            rtc::AsyncSocket* socket,
            const DcSctpOptions& sctp_options)
      : log_prefix_(std::string(name) + ": "),
        clock_(clock),
        thread_(thread),
        udp_socket(std::make_unique<rtc::AsyncUDPSocket>(socket)),
        timeout_factory_(
            *thread_,
            [this]() { return TimeMillis(); },
            [this](dcsctp::TimeoutID timeout_id) {
              sctp_socket_.HandleTimeout(timeout_id);
            }),
        random_(GetUniqueSeed()),
        sctp_socket_(name, *this, MaybeMakePacketObserver(name), sctp_options) {
    udp_socket->SignalReadPacket.connect(this, &SctpActor::OnReadPacket);
  }

  void OnMessage(rtc::Message* pmsg) override {
    if (pmsg->message_id == static_cast<uint32_t>(MessageId::kPrintBandwidth)) {
      TimeMs now = TimeMillis();
      DurationMs duration = now - last_bandwidth_printout_;
      size_t bytes = 0;

      while (!received_messages_.empty()) {
        bytes += received_messages_.front().payload().size();
        received_messages_.pop_front();
      }

      RTC_LOG(LS_INFO) << log_prefix() << "Received "
                       << rtc::StringFormat("%0.1f Mbps",
                                            static_cast<double>(bytes * 8) /
                                                *duration / 1000);

      last_bandwidth_printout_ = now;
      // Print again in a second.
      if (mode_ == ActorMode::kThroughputReceiver) {
        thread_->PostDelayed(RTC_FROM_HERE, 1000, this,
                             static_cast<uint32_t>(MessageId::kPrintBandwidth));
      }
    }
  }

  void OnReadPacket(rtc::AsyncPacketSocket* s,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us) {
    sctp_socket_.ReceivePacket(
        rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data), size));
  }

  // Implement DcSctpSocketCallbacks
  void SendPacket(rtc::ArrayView<const uint8_t> data) override {
    udp_socket->Send(data.data(), data.size(), options);
  }

  std::unique_ptr<Timeout> CreateTimeout() override {
    return timeout_factory_.CreateTimeout();
  }

  TimeMs TimeMillis() override {
    return TimeMs(clock_.TimeNanos() / 1'000'000);
  }

  uint32_t GetRandomInt(uint32_t low, uint32_t high) override {
    return random_.Rand(low, high);
  }

  void OnMessageReceived(DcSctpMessage message) override {
    received_messages_.emplace_back(std::move(message));
  }

  void OnError(ErrorKind error, absl::string_view message) override {
    RTC_LOG(LS_WARNING) << log_prefix() << "Socket error: " << ToString(error)
                        << "; " << message;
  }

  void OnAborted(ErrorKind error, absl::string_view message) override {
    RTC_LOG(LS_WARNING) << log_prefix() << "Socket abort: " << ToString(error)
                        << "; " << message;
  }

  void OnConnected() override {
    RTC_LOG(LS_INFO) << log_prefix() << "OnConnected";
  }

  void OnClosed() override { RTC_LOG(LS_INFO) << log_prefix() << "OnClosed"; }

  void OnConnectionRestarted() override {
    RTC_LOG(LS_INFO) << log_prefix() << "OnConnectionRestarted";
  }

  void OnStreamsResetFailed(rtc::ArrayView<const StreamID> outgoing_streams,
                            absl::string_view reason) override {}

  void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) override {}

  void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) override {}

  void NotifyOutgoingMessageBufferEmpty() override {}

  void OnBufferedAmountLow(StreamID stream_id) override {
    if (mode_ == ActorMode::kThroughputSender) {
      std::vector<uint8_t> payload(kHugePayloadSize);
      sctp_socket_.Send(
          DcSctpMessage(StreamID(1), PPID(53), std::move(payload)),
          SendOptions());
    } else if (mode_ == ActorMode::kLimitedRetransmissionSender) {
      while (sctp_socket_.buffered_amount(StreamID(1)) <
             kBufferedAmountLowThreshold * 2) {
        SendOptions send_options;
        send_options.max_retransmissions = 0;
        sctp_socket_.Send(
            DcSctpMessage(StreamID(1), PPID(53),
                          std::vector<uint8_t>(kLargePayloadSize)),
            send_options);

        send_options.max_retransmissions = absl::nullopt;
        sctp_socket_.Send(
            DcSctpMessage(StreamID(1), PPID(52),
                          std::vector<uint8_t>(kSmallPayloadSize)),
            send_options);
      }
    }
  }

  absl::optional<DcSctpMessage> ConsumeReceivedMessage() {
    if (received_messages_.empty()) {
      return absl::nullopt;
    }
    DcSctpMessage ret = std::move(received_messages_.front());
    received_messages_.pop_front();
    return ret;
  }

  DcSctpSocket& sctp_socket() { return sctp_socket_; }

  void SetActorMode(ActorMode mode) {
    mode_ = mode;
    if (mode_ == ActorMode::kThroughputSender) {
      // Trigger sending.
      sctp_socket_.SetBufferedAmountLowThreshold(StreamID(1), 5000);
      std::vector<uint8_t> payload(kHugePayloadSize);
      sctp_socket_.Send(
          DcSctpMessage(StreamID(1), PPID(53), std::move(payload)),
          SendOptions());
    } else if (mode_ == ActorMode::kLimitedRetransmissionSender) {
      sctp_socket_.SetBufferedAmountLowThreshold(StreamID(1),
                                                 kBufferedAmountLowThreshold);
      std::vector<uint8_t> payload(kHugePayloadSize);
      sctp_socket_.Send(
          DcSctpMessage(StreamID(1), PPID(53), std::move(payload)),
          SendOptions());
    } else if (mode == ActorMode::kThroughputReceiver) {
      thread_->PostDelayed(RTC_FROM_HERE, 1000, this,
                           static_cast<uint32_t>(MessageId::kPrintBandwidth));
    }
  }

 private:
  std::string log_prefix() const {
    rtc::StringBuilder sb;
    sb << log_prefix_;
    sb << (clock_.TimeNanos() / 1000000);
    sb << ": ";
    return sb.Release();
  }

  ActorMode mode_ = ActorMode::kAtRest;
  rtc::PacketOptions options;
  const std::string log_prefix_;
  rtc::ClockInterface& clock_;
  rtc::Thread* thread_;
  std::unique_ptr<rtc::AsyncUDPSocket> udp_socket;
  TaskQueueTimeoutFactory timeout_factory_;
  webrtc::Random random_;
  DcSctpSocket sctp_socket_;
  std::deque<DcSctpMessage> received_messages_;
  TimeMs last_bandwidth_printout_ = TimeMs(0);
};  // namespace

class DcSctpSocketNetworkTest : public testing::Test {
 protected:
  explicit DcSctpSocketNetworkTest(bool enable_message_interleaving = false)
      : options_(MakeOptionsForTest()),
        ss_(&clock_),
        thread_(&ss_),
        send_socket_(ss_.CreateAsyncSocket(kInitialAddr.family(), SOCK_DGRAM)),
        recv_socket_(ss_.CreateAsyncSocket(kInitialAddr.family(), SOCK_DGRAM)),
        sender_("A", clock_, rtc::Thread::Current(), send_socket_, options_),
        receiver_("Z", clock_, rtc::Thread::Current(), recv_socket_, options_) {
    rtc::LogMessage::LogTimestamps();
    RTC_CHECK_EQ(send_socket_->Bind(kInitialAddr), 0);
    RTC_CHECK_EQ(recv_socket_->Bind(kInitialAddr), 0);
    RTC_CHECK_EQ(send_socket_->Connect(recv_socket_->GetLocalAddress()), 0);
    RTC_CHECK_EQ(recv_socket_->Connect(send_socket_->GetLocalAddress()), 0);
  }

  const DcSctpOptions options_;
  rtc::ScopedFakeClock clock_;
  rtc::VirtualSocketServer ss_;
  rtc::AutoSocketServerThread thread_;

  rtc::AsyncSocket* send_socket_;
  rtc::AsyncSocket* recv_socket_;
  SctpActor sender_;
  SctpActor receiver_;
};

TEST_F(DcSctpSocketNetworkTest, CanConnectAndShutdownOverSocketServer) {
  sender_.sctp_socket().Connect();

  ss_.ProcessMessagesUntilIdle();

  sender_.sctp_socket().Shutdown();

  ss_.ProcessMessagesUntilIdle();
}

TEST_F(DcSctpSocketNetworkTest, CanSendLargeMessageOverSocketServer) {
  const uint32_t mean = 30;

  ss_.set_delay_mean(mean);
  ss_.set_delay_stddev(0);
  ss_.UpdateDelayDistribution();

  sender_.sctp_socket().Connect();

  constexpr size_t kPayloadSize = 100 * 1024;

  std::vector<uint8_t> payload(kPayloadSize);
  sender_.sctp_socket().Send(DcSctpMessage(StreamID(1), PPID(53), payload),
                             SendOptions());

  ss_.ProcessMessagesUntilIdle();

  ASSERT_HAS_VALUE_AND_ASSIGN(DcSctpMessage message,
                              receiver_.ConsumeReceivedMessage());

  EXPECT_THAT(message.payload(), SizeIs(kPayloadSize));

  sender_.sctp_socket().Shutdown();

  ss_.ProcessMessagesUntilIdle();
}

TEST_F(DcSctpSocketNetworkTest, CanSendMessagesOnLossyLink) {
  const uint32_t mean = 30;

  ss_.set_delay_mean(mean);
  ss_.set_delay_stddev(0);
  ss_.UpdateDelayDistribution();

  sender_.sctp_socket().Connect();

  ss_.ProcessMessagesUntilIdle();

  sender_.SetActorMode(ActorMode::kLimitedRetransmissionSender);
  receiver_.SetActorMode(ActorMode::kThroughputReceiver);
  ss_.set_drop_probability(0.0001);

  SIMULATED_WAIT(false, 30000, clock_);
  sender_.SetActorMode(ActorMode::kAtRest);
  receiver_.SetActorMode(ActorMode::kAtRest);
  ss_.ProcessMessagesUntilIdle();

  ss_.set_drop_probability(0.0);

  sender_.sctp_socket().Shutdown();

  ss_.ProcessMessagesUntilIdle();
}

// This test is too slow to run in debug mode unfortunately, due to extensive
// consistency check that iterates all outstanding chunks.
#if defined(NDEBUG)
TEST_F(DcSctpSocketNetworkTest, HasHighBandwidthIfSendingWithoutLimit) {
  sender_.sctp_socket().Connect();
  ss_.ProcessMessagesUntilIdle();

  const uint32_t mean = 30;

  ss_.set_delay_mean(mean);
  ss_.set_delay_stddev(0);
  ss_.UpdateDelayDistribution();

  sender_.SetActorMode(ActorMode::kThroughputSender);
  receiver_.SetActorMode(ActorMode::kThroughputReceiver);

  SIMULATED_WAIT(false, 4000, clock_);

  sender_.SetActorMode(ActorMode::kAtRest);
  receiver_.SetActorMode(ActorMode::kAtRest);

  ss_.ProcessMessagesUntilIdle();

  sender_.sctp_socket().Shutdown();

  ss_.ProcessMessagesUntilIdle();
}
#endif
}  // namespace
}  // namespace dcsctp
