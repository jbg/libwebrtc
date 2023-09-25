/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_FUZZERS_DCSCTP_FUZZERS_H_
#define NET_DCSCTP_FUZZERS_DCSCTP_FUZZERS_H_

#include <deque>
#include <memory>
#include <set>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "api/task_queue/task_queue_base.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "rtc_base/random.h"

namespace dcsctp {
namespace dcsctp_fuzzers {

class FuzzerCallbacks : public DcSctpSocketCallbacks {
 public:
  static constexpr int kRandomValue = 42;
  explicit FuzzerCallbacks(absl::string_view name) : name_(name), random_(42) {}
  absl::string_view GetName() const { return name_; }
  void SendPacket(rtc::ArrayView<const uint8_t> data) override {
    sent_packets_.emplace_back(std::vector<uint8_t>(data.begin(), data.end()));
  }
  std::unique_ptr<Timeout> CreateTimeout(
      webrtc::TaskQueueBase::DelayPrecision precision) override {
    // The fuzzer timeouts don't implement |precision|.
    return std::make_unique<FuzzerTimeout>(*this);
  }
  TimeMs TimeMillis() override { return current_time_; }
  uint32_t GetRandomInt(uint32_t low, uint32_t high) override {
    return random_.Rand(low, high);
  }
  void OnMessageReceived(DcSctpMessage message) override;
  void OnError(ErrorKind error, absl::string_view message) override {}
  void OnAborted(ErrorKind error, absl::string_view message) override {
    aborted_ = true;
  }
  void OnConnected() override {}
  void OnClosed() override {}
  void OnConnectionRestarted() override {}
  void OnStreamsResetFailed(rtc::ArrayView<const StreamID> outgoing_streams,
                            absl::string_view reason) override {}
  void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) override {}
  void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) override {}

  std::vector<uint8_t> ConsumeSentPacket() {
    std::vector<uint8_t> packet = GetPacketFromHistory(0);
    if (!packet.empty()) {
      ++sent_packets_read_idx_;
    }
    return packet;
  }

  std::vector<uint8_t> GetPacketFromHistory(int lookback) {
    int idx = sent_packets_read_idx_ - lookback;
    if (idx < 0 || idx >= static_cast<int>(sent_packets_.size())) {
      return {};
    }
    return sent_packets_[idx];
  }

  TimeMs PeekNextExpiryTime() const {
    if (active_timeouts_.empty()) {
      return TimeMs::InfiniteFuture();
    }
    return active_timeouts_.begin()->first;
  }

  absl::optional<TimeoutID> AdvanceTimeTowards(TimeMs max_time) {
    if (active_timeouts_.empty()) {
      current_time_ = max_time;
      return absl::nullopt;
    }

    auto it = active_timeouts_.begin();
    if (it->first > max_time) {
      current_time_ = max_time;
      return absl::nullopt;
    }

    RTC_DCHECK(it->first >= current_time_);
    TimeoutID timeout_id = it->second;
    current_time_ = it->first;
    active_timeouts_.erase(it);
    return timeout_id;
  }

  bool IsAborted() const { return aborted_; }

  uint32_t GetNextMessageId() { return ++next_message_id_; }

 private:
  // A fake timeout used during fuzzing.
  class FuzzerTimeout : public Timeout {
   public:
    explicit FuzzerTimeout(FuzzerCallbacks& parent) : parent_(parent) {}

    void Start(DurationMs duration_ms, TimeoutID timeout_id) override {
      // Start is only allowed to be called on stopped or expired timeouts.
      if (timeout_id_.has_value()) {
        // It has been started before, but maybe it expired. Ensure that it's
        // not running at least.
        RTC_DCHECK_EQ(absl::c_count_if(parent_.active_timeouts_,
                                       [&](const auto& t) {
                                         return t.second == timeout_id;
                                       }),
                      0);
      }
      timeout_id_ = timeout_id;
      TimeMs expiry = parent_.current_time_ + duration_ms;
      RTC_DCHECK(expiry >= parent_.current_time_);
      RTC_DCHECK(
          parent_.active_timeouts_.emplace(std::make_pair(expiry, timeout_id))
              .second);
    }

    void Stop() override {
      // Stop is only allowed to be called on active timeouts. Not stopped or
      // expired.
      RTC_DCHECK(timeout_id_.has_value());
      auto it = absl::c_find_if(parent_.active_timeouts_, [&](const auto& t) {
        return t.second == *timeout_id_;
      });
      RTC_DCHECK(it != parent_.active_timeouts_.end());
      parent_.active_timeouts_.erase(it);
      timeout_id_ = absl::nullopt;
    }

    FuzzerCallbacks& parent_;
    // If present, the timout is active and will expire reported as
    // `timeout_id`.
    absl::optional<TimeoutID> timeout_id_;
  };

  const std::string name_;
  bool aborted_ = false;
  webrtc::Random random_;
  // Needs to be ordered, to allow fuzzers to expire timers.
  TimeMs current_time_ = TimeMs(42);
  std::set<std::pair<TimeMs, TimeoutID>> active_timeouts_;
  std::vector<std::vector<uint8_t>> sent_packets_;
  std::set<uint32_t> received_message_ids_;
  uint32_t next_message_id_ = 0;
  int sent_packets_read_idx_ = 0;
};

inline DcSctpOptions MakeFuzzingOptions() {
  dcsctp::DcSctpOptions options;
  options.disable_checksum_verification = true;
  options.max_retransmissions.reset();
  options.max_init_retransmits.reset();
  return options;
}

struct FuzzedSocket {
  explicit FuzzedSocket(
      absl::string_view name,
      std::unique_ptr<PacketObserver> packet_observer = nullptr)
      : options(MakeFuzzingOptions()),
        cb(name),
        socket(name, cb, std::move(packet_observer), options) {}

  const DcSctpOptions options;
  FuzzerCallbacks cb;
  DcSctpSocket socket;
};

// Given some fuzzing `data` will send packets to the socket as well as calling
// API methods.
void FuzzSocket(FuzzedSocket& socket, rtc::ArrayView<const uint8_t> data);

void FuzzConnection(FuzzedSocket& a,
                    FuzzedSocket& z,
                    rtc::ArrayView<const uint8_t> data);

struct FuzzCommandAdvanceTime {};
struct FuzzCommandReceivePackets {
  bool a_to_z;
  size_t count;
};
struct FuzzCommandDropPacket {
  bool socket_is_a;
};
struct FuzzCommandRetransmitPacket {
  bool a_to_z;
  size_t lookback;
};
struct FuzzCommandSendMessage {
  bool socket_is_a;
  int stream_id;
  bool unordered;
  int max_retransmissions;
  size_t message_size;
};
struct FuzzCommandResetStream {
  bool socket_is_a;
  bool reset_1;
  bool reset_2;
};
using FuzzCommand = std::variant<FuzzCommandAdvanceTime,
                                 FuzzCommandReceivePackets,
                                 FuzzCommandDropPacket,
                                 FuzzCommandRetransmitPacket,
                                 FuzzCommandSendMessage,
                                 FuzzCommandResetStream>;

std::vector<FuzzCommand> MakeFuzzCommands(rtc::ArrayView<const uint8_t> data);
std::string PrintFuzzCommands(rtc::ArrayView<const FuzzCommand> commands);
void ExecuteFuzzCommands(FuzzedSocket& a,
                         FuzzedSocket& z,
                         rtc::ArrayView<const FuzzCommand> commands);

}  // namespace dcsctp_fuzzers
}  // namespace dcsctp
#endif  // NET_DCSCTP_FUZZERS_DCSCTP_FUZZERS_H_
