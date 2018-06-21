/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_SIMULATED_NETWORK_H_
#define API_TEST_SIMULATED_NETWORK_H_

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <queue>
#include <vector>

#include "api/optional.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/random.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

struct PacketInFlightInfo {
  PacketInFlightInfo(size_t size, int64_t send_time_us, uint64_t packet_id)
      : size(size), send_time_us(send_time_us), packet_id(packet_id) {}

  size_t size;
  int64_t send_time_us;
  // Unique identifier for the packet in relation to other packets in flight.
  uint64_t packet_id;
};

struct PacketDeliveryInfo {
  static constexpr int kNotReceived = -1;
  PacketDeliveryInfo(PacketInFlightInfo source, int64_t receive_time_us)
      : receive_time_us(receive_time_us), packet_id(source.packet_id) {}
  int64_t receive_time_us;
  uint64_t packet_id;
};

class NetworkSimulationInterface {
 public:
  virtual bool EnqueuePacket(PacketInFlightInfo packet_info) = 0;
  // Retrieves all packets that should be delivered by the given receive time.
  virtual std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) = 0;
  virtual absl::optional<int64_t> NextDeliveryTimeUs() const = 0;
  virtual ~NetworkSimulationInterface() = default;
};

// Class simulating a network link. This is a simple and naive solution just
// faking capacity and adding an extra transport delay in addition to the
// capacity introduced delay.
class SimulatedNetwork : public NetworkSimulationInterface {
 public:
  struct Config {
    Config() {}
    // Queue length in number of packets.
    size_t queue_length_packets = 0;
    // Delay in addition to capacity induced delay.
    int queue_delay_ms = 0;
    // Standard deviation of the extra delay.
    int delay_standard_deviation_ms = 0;
    // Link capacity in kbps.
    int link_capacity_kbps = 0;
    // Random packet loss.
    int loss_percent = 0;
    // If packets are allowed to be reordered.
    bool allow_reordering = false;
    // The average length of a burst of lost packets.
    int avg_burst_loss_length = -1;
  };
  explicit SimulatedNetwork(Config config, uint64_t random_seed = 1);

  // Sets a new configuration. This won't affect packets already in the pipe.
  void SetConfig(const Config& config);

  // NetworkSimulationInterface
  bool EnqueuePacket(PacketInFlightInfo packet) override;
  std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) override;

  absl::optional<int64_t> NextDeliveryTimeUs() const override;

 private:
  struct PacketInfo {
    PacketInFlightInfo packet;
    int64_t arrival_time_us;
  };
  rtc::CriticalSection config_lock_;

  // |process_lock| guards the data structures involved in delay and loss
  // processes, such as the packet queues.
  rtc::CriticalSection process_lock_;
  std::queue<PacketInfo> capacity_link_ RTC_GUARDED_BY(process_lock_);
  Random random_;

  std::deque<PacketInfo> delay_link_;

  // Link configuration.
  Config config_ RTC_GUARDED_BY(config_lock_);

  // Are we currently dropping a burst of packets?
  bool bursting_;

  // The probability to drop the packet if we are currently dropping a
  // burst of packet
  double prob_loss_bursting_ RTC_GUARDED_BY(config_lock_);

  // The probability to drop a burst of packets.
  double prob_start_bursting_ RTC_GUARDED_BY(config_lock_);
  int64_t capacity_delay_error_bytes_ = 0;
};

}  // namespace webrtc

#endif  // API_TEST_SIMULATED_NETWORK_H_
