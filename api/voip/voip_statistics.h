/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_STATISTICS_H_
#define API_VOIP_VOIP_STATISTICS_H_

#include "api/voip/voip_base.h"

namespace webrtc {

struct DecodingStatistics {
  // Number of calls where silence generated and NetEq was disengaged from
  // decoding.
  int calls_to_silence_generator = 0;
  // Number of calls to NetEq.
  int calls_to_neteq = 0;
  // Number of calls where audio RTP packet decoded.
  int decoded_normal = 0;
  // Number of calls resulted in NetEq PLC.
  int decoded_neteq_plc = 0;
  // Number of calls resulted in codec PLC.
  int decoded_codec_plc = 0;
  // Number of calls where comfort noise generated due to DTX.
  int decoded_cng = 0;
  // Number of calls resulted where PLC faded to CNG.
  int decoded_plc_cng = 0;
  // Number of calls returning a muted state output.
  int decoded_muted_output = 0;
};

struct NetEqStatistics {
  // LifeTimeTracked persists over the lifetime of the media session.
  struct LifeTimeTracked {
    // Stats below correspond to similarly-named fields in the WebRTC stats
    // spec. https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats
    uint64_t total_samples_received = 0;
    uint64_t concealed_samples = 0;
    uint64_t concealment_events = 0;
    uint64_t jitter_buffer_delay_ms = 0;
    uint64_t jitter_buffer_emitted_count = 0;
    uint64_t jitter_buffer_target_delay_ms = 0;
    uint64_t inserted_samples_for_deceleration = 0;
    uint64_t removed_samples_for_acceleration = 0;
    uint64_t silent_concealed_samples = 0;
    uint64_t fec_packets_received = 0;
    uint64_t fec_packets_discarded = 0;
    // A delayed packet outage event is defined as an expand period caused not
    // by an actual packet loss, but by a delayed packet.
    uint64_t delayed_packet_outage_samples = 0;
    // This is sum of relative packet arrival delays of received packets so far.
    // Since end-to-end delay of a packet is difficult to measure and is not
    // necessarily useful for measuring jitter buffer performance, we report a
    // relative packet arrival delay. The relative packet arrival delay of a
    // packet is defined as the arrival delay compared to the first packet
    // received, given that it had zero delay. To avoid clock drift, the "first"
    // packet can be made dynamic.
    uint64_t relative_packet_arrival_delay_ms = 0;
    // An interruption is a loss-concealment event lasting at least 150 ms. The
    // two stats below count the number of such events and the total duration of
    // these events.
    int interruption_count = 0;
    int total_interruption_duration_ms = 0;
    // Count of the number of buffer flushes.
    uint64_t packet_buffer_flushes = 0;
  };

  // Current jitter buffer size in ms.
  int current_buffer_size_ms = 0;
  // Target buffer size in ms.
  int preferred_buffer_size_ms = 0;
  // True if adding extra delay due to peaky jitter; false otherwise.
  bool jitter_peaks_found = false;
  // Fraction (of original stream) of synthesized audio inserted through
  // expansion (in Q14).
  int expand_rate = 0;
  // Fraction (of original stream) of synthesized speech inserted through
  // expansion (in Q14).
  int speech_expand_rate = 0;
  // Fraction of data inserted through pre-emptive expansion (in Q14).
  int preemptive_rate = 0;
  // Fraction of data removed through acceleration (in Q14).
  int accelerate_rate = 0;
  // Fraction of data coming from FEC/RED decoding (in Q14).
  int secondary_decoded_rate = 0;
  // Fraction of discarded FEC/RED data (in Q14).
  int secondary_discarded_rate = 0;
  // Statistics for packet waiting times, i.e., the time between a packet
  // arrives until it is decoded.
  int mean_waiting_time_ms = 0;
  int median_waiting_time_ms = 0;
  int min_waiting_time_ms = 0;
  int max_waiting_time_ms = 0;

  LifeTimeTracked life_time;
};

// VoipStatistics interface provides the interfaces for querying metrics around
// audio decoding module and jitter buffer (NetEq) performance.
class VoipStatistics {
 public:
  // Gets the statistics on ACM (Audio Coding Module) decoding performance which
  // will reset after each query.
  virtual absl::optional<DecodingStatistics> GetDecodingStatistics(
      ChannelId channel_id) = 0;

  // Gets the statistics from NetEq. The members not included in LifeTimeTracked
  // struct are reset after each query.
  virtual absl::optional<NetEqStatistics> GetNetEqStatistics(
      ChannelId channel_id) = 0;

 protected:
  virtual ~VoipStatistics() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_STATISTICS_H_
