/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/experiments/field_trial_list.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

bool IsValid(DataRate datarate) {
  return datarate.IsFinite();
}

bool IsValid(Timestamp timestamp) {
  return timestamp.IsFinite();
}

DataSize GetDataSize(rtc::ArrayView<const PacketResult> packet_results) {
  return absl::c_accumulate(
      packet_results, DataSize::Zero(),
      [](DataSize s, PacketResult p) { return s + p.sent_packet.size; });
}

// Returns `PlusInfinity` if `packet_results` is empty.
Timestamp GetFirstSendTime(rtc::ArrayView<const PacketResult> packet_results) {
  if (packet_results.empty()) {
    return Timestamp::PlusInfinity();
  }
  return absl::c_min_element(packet_results,
                             [](PacketResult p1, PacketResult p2) {
                               return p1.sent_packet.send_time <
                                      p2.sent_packet.send_time;
                             })
      ->sent_packet.send_time;
}

// Returns `MinusInfinity` if `packet_results` is empty.
Timestamp GetLastSendTime(rtc::ArrayView<const PacketResult> packet_results) {
  if (packet_results.empty()) {
    return Timestamp::MinusInfinity();
  }
  return absl::c_max_element(packet_results,
                             [](PacketResult p1, PacketResult p2) {
                               return p1.sent_packet.send_time <
                                      p2.sent_packet.send_time;
                             })
      ->sent_packet.send_time;
}

int GetNumberOfLostPackets(rtc::ArrayView<const PacketResult> packet_results) {
  return absl::c_count_if(packet_results,
                          [](PacketResult p) { return !p.IsReceived(); });
}

double GetLossProbability(double inherent_loss,
                          DataRate loss_limited_bandwidth,
                          DataRate sending_rate) {
  RTC_LOG_F(LS_WARNING) << "The inherent loss must be in [0,1]: "
                        << inherent_loss;
  RTC_LOG_F(LS_WARNING) << "The sending rate must be finite: "
                        << ToString(sending_rate);
  RTC_LOG_F(LS_WARNING) << "The loss limited bandwidth must be finite: "
                        << ToString(loss_limited_bandwidth);

  inherent_loss = std::min(std::max(inherent_loss, 0.0), 1.0);

  double loss_probability = inherent_loss;
  if (IsValid(sending_rate) && IsValid(loss_limited_bandwidth) &&
      (sending_rate > loss_limited_bandwidth)) {
    loss_probability += (sending_rate - loss_limited_bandwidth) / sending_rate;
  }

  return std::min(loss_probability, 1.0);
}

LossBasedBweV2::Config CreateConfig(
    const WebRtcKeyValueConfig* key_value_config) {
  FieldTrialParameter<bool> enabled("Enabled", false);

  FieldTrialParameter<double> bw_rampup_upper_bound_factor(
      "BandwidthRampupUpperBoundFactor", 1.1);
  FieldTrialList<double> candidate_factors("CandidateFactors",
                                           {1.05, 1.0, 0.95});
  FieldTrialParameter<double> higher_bw_bias_factor("HigherBandwidthBiasFactor",
                                                    0.00001);
  FieldTrialParameter<double> inherent_loss_lower_bound(
      "InherentLossLowerBound", 1.0e-3);
  FieldTrialParameter<DataRate> inherent_loss_upper_bound_bw_balance(
      "InherentLossUpperBoundBandwidthBalance", DataRate::KilobitsPerSec(15.0));
  FieldTrialParameter<double> inherent_loss_upper_bound_offset(
      "InherentLossUpperBoundOffset", 0.05);
  FieldTrialParameter<double> initial_inherent_loss_estimate(
      "InitialInherentLossEstimate", 0.01);
  FieldTrialParameter<int> newton_iterations("NewtonIterations", 1);
  FieldTrialParameter<double> newton_step_size("NewtonStepSize", 0.5);
  FieldTrialParameter<TimeDelta> observation_duration_lower_bound(
      "ObservationDurationLowerBound", TimeDelta::Seconds(1));
  FieldTrialParameter<int> observation_window_size("ObservationWindowSize", 20);
  FieldTrialParameter<double> sending_rate_smoothing_factor(
      "SendingRateSmoothingFactor", 0.0);
  FieldTrialParameter<double> tcp_fairness_temporal_weight_factor(
      "TcpFairnessTemporalWeightFactor", 0.99);
  FieldTrialParameter<DataRate> tcp_fairness_upper_bound_bw_balance(
      "TcpFairnessUpperBoundBwBalance", DataRate::KilobitsPerSec(15.0));
  FieldTrialParameter<double> tcp_fairness_upper_bound_loss_offset(
      "TcpFairnessUpperBoundLossOffset", 0.05);
  FieldTrialParameter<double> temporal_weight_factor("TemporalWeightFactor",
                                                     0.99);

  ParseFieldTrial(
      {&enabled, &bw_rampup_upper_bound_factor, &candidate_factors,
       &higher_bw_bias_factor, &inherent_loss_lower_bound,
       &inherent_loss_upper_bound_bw_balance, &inherent_loss_upper_bound_offset,
       &initial_inherent_loss_estimate, &newton_iterations, &newton_step_size,
       &observation_duration_lower_bound, &observation_window_size,
       &sending_rate_smoothing_factor, &tcp_fairness_temporal_weight_factor,
       &tcp_fairness_upper_bound_bw_balance,
       &tcp_fairness_upper_bound_loss_offset, &temporal_weight_factor},
      key_value_config->Lookup("WebRTC-Bwe-LossBasedBweV2"));

  LossBasedBweV2::Config config;

  config.enabled = enabled.Get();
  config.bw_rampup_upper_bound_factor = bw_rampup_upper_bound_factor.Get();
  config.candidate_factors = candidate_factors.Get();
  config.higher_bw_bias_factor = higher_bw_bias_factor.Get();
  config.inherent_loss_lower_bound = inherent_loss_lower_bound.Get();
  config.inherent_loss_upper_bound_bw_balance =
      inherent_loss_upper_bound_bw_balance.Get();
  config.inherent_loss_upper_bound_offset =
      inherent_loss_upper_bound_offset.Get();
  config.initial_inherent_loss_estimate = initial_inherent_loss_estimate.Get();
  config.newton_iterations = newton_iterations.Get();
  config.newton_step_size = newton_step_size.Get();
  config.observation_duration_lower_bound =
      observation_duration_lower_bound.Get();
  config.observation_window_size = observation_window_size.Get();
  config.sending_rate_smoothing_factor = sending_rate_smoothing_factor.Get();
  config.tcp_fairness_temporal_weight_factor =
      tcp_fairness_temporal_weight_factor.Get();
  config.tcp_fairness_upper_bound_bw_balance =
      tcp_fairness_upper_bound_bw_balance.Get();
  config.tcp_fairness_upper_bound_loss_offset =
      tcp_fairness_upper_bound_loss_offset.Get();
  config.temporal_weight_factor = temporal_weight_factor.Get();

  return config;
}

bool IsValid(const LossBasedBweV2::Config& config) {
  bool valid = true;

  if (config.bw_rampup_upper_bound_factor <= 1.0) {
    RTC_LOG_F(LS_WARNING)
        << "The bandwidth rampup upper bound factor must be greater than 1: "
        << config.bw_rampup_upper_bound_factor;
    valid = false;
  }

  if (config.higher_bw_bias_factor < 0.0) {
    RTC_LOG_F(LS_WARNING)
        << "The higher bandwidth bias factor must be non-negative: "
        << config.higher_bw_bias_factor;
    valid = false;
  }

  if (config.inherent_loss_lower_bound < 0.0 ||
      config.inherent_loss_lower_bound >= 1.0) {
    RTC_LOG_F(LS_WARNING) << "The inherent loss lower bound must be in [0, 1): "
                          << config.inherent_loss_lower_bound;
    valid = false;
  }

  if (config.inherent_loss_upper_bound_bw_balance <= DataRate::Zero()) {
    RTC_LOG_F(LS_WARNING) << "The inherent loss upper bound bandwidth balance "
                             "must be positive: "
                          << ToString(
                                 config.inherent_loss_upper_bound_bw_balance);
    valid = false;
  }

  if (config.inherent_loss_upper_bound_offset <
          config.inherent_loss_lower_bound ||
      config.inherent_loss_upper_bound_offset >= 1.0) {
    RTC_LOG_F(LS_WARNING) << "The inherent loss upper bound must be greater "
                             "than or equal to the inherent "
                             "loss lower bound, which is "
                          << config.inherent_loss_lower_bound
                          << ", and less than 1: "
                          << config.inherent_loss_upper_bound_offset;
    valid = false;
  }

  if (config.initial_inherent_loss_estimate < 0.0 ||
      config.initial_inherent_loss_estimate >= 1.0) {
    RTC_LOG_F(LS_WARNING)

        << "The initial inherent loss estimate must be in [0, 1): "
        << config.initial_inherent_loss_estimate;
    valid = false;
  }

  if (config.newton_iterations <= 0) {
    RTC_LOG_F(LS_WARNING)
        << "The number of Newton iterations must be positive: "
        << config.newton_iterations;
    valid = false;
  }

  if (config.newton_step_size <= 0.0) {
    RTC_LOG_F(LS_WARNING) << "The Newton step size must be positive: "
                          << config.newton_step_size;
    valid = false;
  }

  if (config.observation_duration_lower_bound <= TimeDelta::Zero()) {
    RTC_LOG_F(LS_WARNING)
        << "The observation duration lower bound must be positive: "
        << ToString(config.observation_duration_lower_bound);
    valid = false;
  }

  if (config.observation_window_size < 2) {
    RTC_LOG_F(LS_WARNING) << "The observation window size must be at least 2: "
                          << config.observation_window_size;
    valid = false;
  }

  if (config.sending_rate_smoothing_factor < 0.0 ||
      config.sending_rate_smoothing_factor >= 1.0) {
    RTC_LOG_F(LS_WARNING)
        << "The sending rate smoothing factor must be in [0, 1): "
        << config.sending_rate_smoothing_factor;
    valid = false;
  }

  if (config.tcp_fairness_temporal_weight_factor <= 0.0 ||
      config.tcp_fairness_temporal_weight_factor > 1.0) {
    RTC_LOG_F(LS_WARNING)
        << "The TCP fairness temporal weight factor must be in (0, 1]"
        << config.tcp_fairness_temporal_weight_factor;
    valid = false;
  }

  if (config.tcp_fairness_upper_bound_bw_balance <= DataRate::Zero()) {
    RTC_LOG_F(LS_WARNING)
        << "The TCP fairness upper bound bandwidth balance must be positive: "
        << ToString(config.tcp_fairness_upper_bound_bw_balance);
    valid = false;
  }

  if (config.tcp_fairness_upper_bound_loss_offset < 0.0 ||
      config.tcp_fairness_upper_bound_loss_offset >= 1.0) {
    RTC_LOG_F(LS_WARNING)
        << "The TCP fairness upper bound loss offset must be in [0, 1): "
        << config.tcp_fairness_upper_bound_loss_offset;
    valid = false;
  }

  if (config.temporal_weight_factor <= 0.0 ||
      config.temporal_weight_factor > 1.0) {
    RTC_LOG_F(LS_WARNING) << "The temporal weight factor must be in (0, 1]: "
                          << config.temporal_weight_factor;
    valid = false;
  }
  return valid;
}

}  // namespace

LossBasedBweV2 LossBasedBweV2::Create(
    const WebRtcKeyValueConfig* key_value_config) {
  Config config = CreateConfig(key_value_config);
  if (!IsValid(config)) {
    RTC_LOG_F(LS_WARNING)
        << "The loss based bandwidth estimator v2 is disabled due to the "
           "configuration not being valid.";
    config.enabled = false;
  }
  return LossBasedBweV2(std::move(config));
}

LossBasedBweV2::LossBasedBweV2(Config config) : config_(std::move(config)) {
  current_estimate_.inherent_loss = config_.initial_inherent_loss_estimate;
  observations_.resize(config_.observation_window_size);
  temporal_weights_.resize(config_.observation_window_size);
  tcp_fairness_temporal_weights_.resize(config_.observation_window_size);
  CalculateTemporalWeights();
}

bool LossBasedBweV2::IsEnabled() const {
  return config_.enabled;
}

bool LossBasedBweV2::IsReady() const {
  return IsValid(current_estimate_.loss_limited_bandwidth) &&
         num_observations_ > 0;
}

DataRate LossBasedBweV2::GetBandwidthEstimate() const {
  if (!IsReady()) {
    RTC_LOG_F(LS_WARNING) << "No bandwidth estimate has been made.";
    return DataRate::PlusInfinity();
  }

  return std::min(current_estimate_.loss_limited_bandwidth,
                  GetTcpFairnessBwUpperBound());
}

void LossBasedBweV2::SetAcknowledgedBitrate(DataRate acknowledged_bitrate) {
  if (IsValid(acknowledged_bitrate)) {
    acknowledged_bitrate_ = acknowledged_bitrate;
  } else {
    RTC_LOG_F(LS_WARNING) << "The acknowledged bitrate must be finite: "
                          << ToString(acknowledged_bitrate);
  }
}

void LossBasedBweV2::SetInitialBwPrediction(DataRate bwe) {
  if (IsValid(bwe)) {
    current_estimate_.loss_limited_bandwidth = bwe;
  } else {
    RTC_LOG_F(LS_WARNING) << "The initial bandwidth prediction must be finite: "
                          << ToString(bwe);
  }
}

void LossBasedBweV2::UpdateBwe(
    rtc::ArrayView<const PacketResult> packet_results) {
  if (packet_results.empty()) {
    return;
  }

  if (!PushBackObservation(packet_results)) {
    return;
  }

  if (!IsValid(current_estimate_.loss_limited_bandwidth)) {
    return;
  }

  ChannelParameters best_candidate = current_estimate_;
  double objective_max = std::numeric_limits<double>::lowest();

  for (ChannelParameters candidate : GetCandidates()) {
    NewtonsMethodUpdate(candidate, GetMostRecentObservation().id);

    const double candidate_objective =
        GetObjective(candidate, GetMostRecentObservation().id);

    if (candidate_objective > objective_max) {
      objective_max = candidate_objective;
      best_candidate = candidate;
    }
  }

  current_estimate_ = best_candidate;
}

double LossBasedBweV2::GetAverageReportedLossRatio() const {
  if (num_observations_ < 1) {
    RTC_LOG_F(LS_WARNING) << "No observations have been registred.";
    return 0.0;
  }

  int number_of_packets = 0;
  int number_of_lost_packets = 0;
  const int id = GetMostRecentObservation().id;

  for (const Observation& observation : observations_) {
    if (!observation.IsInitialized()) {
      continue;
    }
    double tcp_fairness_temporal_weight =
        tcp_fairness_temporal_weights_[id - observation.id];
    number_of_packets +=
        tcp_fairness_temporal_weight * observation.number_of_packets;
    number_of_lost_packets +=
        tcp_fairness_temporal_weight * observation.number_of_lost_packets;
  }

  return static_cast<double>(number_of_lost_packets) / number_of_packets;
}

std::vector<LossBasedBweV2::ChannelParameters> LossBasedBweV2::GetCandidates()
    const {
  std::vector<DataRate> bandwidths;
  for (double candidate_factor : config_.candidate_factors) {
    bandwidths.emplace_back(candidate_factor *
                            current_estimate_.loss_limited_bandwidth);
  }

  if (acknowledged_bitrate_.has_value()) {
    bandwidths.emplace_back(*acknowledged_bitrate_);
  }

  // TODO(crodbro): Consider adding the `delay_based_estimate` as a candidate.

  std::vector<ChannelParameters> candidates;
  candidates.resize(bandwidths.size());

  const DataRate candidate_bw_upper_bound =
      acknowledged_bitrate_.has_value()
          ? config_.bw_rampup_upper_bound_factor * (*acknowledged_bitrate_)
          : DataRate::PlusInfinity();

  for (size_t i = 0; i < bandwidths.size(); ++i) {
    ChannelParameters candidate = current_estimate_;
    candidate.loss_limited_bandwidth = std::min(
        bandwidths[i], std::max(current_estimate_.loss_limited_bandwidth,
                                candidate_bw_upper_bound));
    candidate.inherent_loss = GetFeasibleInherentLoss(candidate);
    candidates[i] = candidate;
  }

  return candidates;
}

LossBasedBweV2::Derivatives LossBasedBweV2::GetDerivatives(
    const ChannelParameters& channel_parameters,
    int id) const {
  Derivatives derivatives;

  for (const Observation& observation : observations_) {
    if (!observation.IsInitialized()) {
      continue;
    }

    double loss_probability = GetLossProbability(
        channel_parameters.inherent_loss,
        channel_parameters.loss_limited_bandwidth, observation.sending_rate);

    double temporal_weight = temporal_weights_[id - observation.id];

    derivatives.first +=
        temporal_weight *
        ((observation.number_of_lost_packets / loss_probability) -
         (observation.number_of_received_packets / (1 - loss_probability)));
    derivatives.second -=
        temporal_weight *
        ((observation.number_of_lost_packets / std::pow(loss_probability, 2)) +
         (observation.number_of_received_packets /
          std::pow(1 - loss_probability, 2)));
  }

  return derivatives;
}

double LossBasedBweV2::GetFeasibleInherentLoss(
    const ChannelParameters& channel_parameters) const {
  return std::min(
      std::max(channel_parameters.inherent_loss,
               config_.inherent_loss_lower_bound),
      GetInherentLossUpperBound(channel_parameters.loss_limited_bandwidth));
}

double LossBasedBweV2::GetInherentLossUpperBound(DataRate bw) const {
  return config_.inherent_loss_upper_bound_offset +
         config_.inherent_loss_upper_bound_bw_balance / bw;
}

LossBasedBweV2::Observation LossBasedBweV2::GetMostRecentObservation() const {
  if (num_observations_ < 1) {
    RTC_LOG_F(LS_WARNING) << "No observations have been registered.";
    return Observation();
  }
  return observations_[(num_observations_ - 1) %
                       config_.observation_window_size];
}

double LossBasedBweV2::GetObjective(const ChannelParameters& channel_parameters,
                                    int id) const {
  double objective = 0.0;
  for (const Observation& observation : observations_) {
    if (!observation.IsInitialized()) {
      continue;
    }

    double loss_probability = GetLossProbability(
        channel_parameters.inherent_loss,
        channel_parameters.loss_limited_bandwidth, observation.sending_rate);

    double temporal_weight = temporal_weights_[id - observation.id];

    objective +=
        temporal_weight *
        ((observation.number_of_lost_packets * std::log(loss_probability)) +
         (observation.number_of_received_packets *
          std::log(1 - loss_probability)));
    objective +=
        temporal_weight * (config_.higher_bw_bias_factor *
                           channel_parameters.loss_limited_bandwidth.kbps() *
                           observation.number_of_packets);
  }
  return objective;
}

DataRate LossBasedBweV2::GetSendingRate(
    DataRate instantaneous_sending_rate,
    DataRate sending_rate_previous_observation) const {
  return config_.sending_rate_smoothing_factor *
             sending_rate_previous_observation +
         (1 - config_.sending_rate_smoothing_factor) *
             instantaneous_sending_rate;
}

DataRate LossBasedBweV2::GetTcpFairnessBwUpperBound() const {
  if (num_observations_ < 1) {
    RTC_LOG_F(LS_WARNING) << "No observations have been registred.";
    return DataRate::PlusInfinity();
  }
  const double average_reported_loss_ratio = GetAverageReportedLossRatio();

  if (average_reported_loss_ratio <=
      config_.tcp_fairness_upper_bound_loss_offset) {
    return DataRate::PlusInfinity();
  }

  return config_.tcp_fairness_upper_bound_bw_balance /
         (average_reported_loss_ratio -
          config_.tcp_fairness_upper_bound_loss_offset);
}

void LossBasedBweV2::CalculateTemporalWeights() {
  for (int i = 0; i < config_.observation_window_size; ++i) {
    temporal_weights_[i] = std::pow(config_.temporal_weight_factor, i);
    tcp_fairness_temporal_weights_[i] =
        std::pow(config_.tcp_fairness_temporal_weight_factor, i);
  }
}

void LossBasedBweV2::NewtonsMethodUpdate(ChannelParameters& channel_parameters,
                                         int id) const {
  for (int i = 0; i < config_.newton_iterations; ++i) {
    const Derivatives derivatives = GetDerivatives(channel_parameters, id);
    channel_parameters.inherent_loss -=
        config_.newton_step_size * derivatives.first / derivatives.second;
    channel_parameters.inherent_loss =
        GetFeasibleInherentLoss(channel_parameters);
  }
}

bool LossBasedBweV2::PushBackObservation(
    rtc::ArrayView<const PacketResult> packet_results) {
  RTC_DCHECK(!packet_results.empty());
  if (packet_results.empty()) {
    return false;
  }

  partial_observation_.number_of_packets += packet_results.size();
  partial_observation_.number_of_lost_packets +=
      GetNumberOfLostPackets(packet_results);
  partial_observation_.size += GetDataSize(packet_results);

  // This is the first packet report we have received.
  if (!IsValid(t_max_previous_observation_)) {
    t_max_previous_observation_ = GetFirstSendTime(packet_results);
  }

  const Timestamp t_max = GetLastSendTime(packet_results);
  const TimeDelta dt = t_max - t_max_previous_observation_;

  // Too small to be meaningful.
  if (dt < config_.observation_duration_lower_bound) {
    return false;
  }

  t_max_previous_observation_ = t_max;

  Observation o;
  o.number_of_packets = partial_observation_.number_of_packets;
  o.number_of_lost_packets = partial_observation_.number_of_lost_packets;
  o.number_of_received_packets = o.number_of_packets - o.number_of_lost_packets;

  const DataRate instantaneous_sending_rate = partial_observation_.size / dt;

  o.sending_rate = (num_observations_ > 0)
                       ? GetSendingRate(instantaneous_sending_rate,
                                        GetMostRecentObservation().sending_rate)
                       : instantaneous_sending_rate;

  o.id = num_observations_++;
  observations_[o.id % config_.observation_window_size] = o;

  ResetPartialObservation();
  return true;
}

void LossBasedBweV2::ResetPartialObservation() {
  partial_observation_.number_of_packets = 0;
  partial_observation_.number_of_lost_packets = 0;
  partial_observation_.size = DataSize::Zero();
}

}  // namespace webrtc
