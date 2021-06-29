/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_V2_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_V2_H_
#include <cstddef>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

class LossBasedBweV2 {
 public:
  struct Config {
    bool enabled = false;

    double bw_rampup_upper_bound_factor = 0.0;
    std::vector<double> candidate_factors;
    double higher_bw_bias_factor = 0.0;
    double inherent_loss_lower_bound = 0.0;
    DataRate inherent_loss_upper_bound_bw_balance = DataRate::MinusInfinity();
    double inherent_loss_upper_bound_offset = 0.0;
    double initial_inherent_loss_estimate = 0.0;
    int newton_iterations = 0;
    double newton_step_size = 0.0;
    TimeDelta observation_duration_lower_bound = TimeDelta::Zero();
    int observation_window_size = 0;
    double sending_rate_smoothing_factor = 0.0;
    double tcp_fairness_temporal_weight_factor = 0.0;
    DataRate tcp_fairness_upper_bound_bw_balance = DataRate::MinusInfinity();
    double tcp_fairness_upper_bound_loss_offset = 0.0;
    double temporal_weight_factor = 0.0;
  };

  // Returns a disabled `LossBasedBweV2` if the `key_value_config` is not valid.
  static LossBasedBweV2 Create(const WebRtcKeyValueConfig* key_value_config);

  ~LossBasedBweV2() = default;

  bool IsEnabled() const;
  bool IsReady() const;

  // Returns `DataRate::PlusInfinity` if no BWE can be calculated.
  DataRate GetBandwidthEstimate() const;

  void SetAcknowledgedBitrate(DataRate acknowledged_bitrate);
  void SetInitialBwPrediction(DataRate bwe);

  void UpdateBwe(rtc::ArrayView<const PacketResult> packet_results);

 private:
  struct ChannelParameters {
    double inherent_loss = 0.0;
    DataRate loss_limited_bandwidth = DataRate::MinusInfinity();
  };

  struct Derivatives {
    double first = 0.0;
    double second = 0.0;
  };

  struct Observation {
    bool IsInitialized() const { return id != -1; }

    int number_of_packets = 0;
    int number_of_lost_packets = 0;
    int number_of_received_packets = 0;
    DataRate sending_rate = DataRate::MinusInfinity();
    int id = -1;
  };

  struct PartialObservation {
    int number_of_packets = 0;
    int number_of_lost_packets = 0;
    DataSize size = DataSize::Zero();
  };

  explicit LossBasedBweV2(Config config);

  // Returns `0.0` if not enough loss statistics have been received.
  double GetAverageReportedLossRatio() const;
  std::vector<ChannelParameters> GetCandidates() const;
  Derivatives GetDerivatives(const ChannelParameters& channel_parameters,
                             int id) const;
  double GetFeasibleInherentLoss(
      const ChannelParameters& channel_parameters) const;
  double GetInherentLossUpperBound(DataRate bw) const;
  // Returns a default `Observation` if no `Observation`s have been registered.
  Observation GetMostRecentObservation() const;
  double GetObjective(const ChannelParameters& channel_parameters,
                      int id) const;
  DataRate GetSendingRate(DataRate instantaneous_sending_rate,
                          DataRate sending_rate_previous_observation) const;
  DataRate GetTcpFairnessBwUpperBound() const;

  void CalculateTemporalWeights();
  void NewtonsMethodUpdate(ChannelParameters& channel_parameters, int id) const;
  // Returns false if no observation was created.
  bool PushBackObservation(rtc::ArrayView<const PacketResult> packet_results);
  void ResetPartialObservation();

  absl::optional<DataRate> acknowledged_bitrate_;
  Config config_;
  ChannelParameters current_estimate_;
  int num_observations_ = 0;
  std::vector<Observation> observations_;
  PartialObservation partial_observation_;
  Timestamp t_max_previous_observation_ = Timestamp::PlusInfinity();
  std::vector<double> tcp_fairness_temporal_weights_;
  std::vector<double> temporal_weights_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_V2_H_
