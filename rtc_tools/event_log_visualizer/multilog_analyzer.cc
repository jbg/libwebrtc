/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/multilog_analyzer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"

namespace webrtc {

namespace {

const int kNumMicrosecsPerSec = 1000000;

bool CompareTimeSeriesPoint(const TimeSeriesPoint& lhs,
                            const TimeSeriesPoint& rhs) {
  return lhs.x < rhs.x;
}

std::string ConnectionIdString(const IceTransaction::ConnectionId& id) {
  return std::to_string(id.first) + ", " + std::to_string(id.second);
}

std::string JoinCandidatePairIds(const std::set<uint32_t>& ids) {
  rtc::StringBuilder sb;
  for (auto itr = ids.begin(); itr != ids.end();) {
    sb << *itr;
    if (++itr != ids.end()) {
      sb << ", ";
    }
  }
  return sb.Release();
}

}  // namespace

PlotLimits::PlotLimits()
    : min_(std::numeric_limits<float>::max()),
      max_(std::numeric_limits<float>::min()) {}

void PlotLimits::Update(float value) {
  min_ = std::min(min_, value);
  max_ = std::max(max_, value);
}

IceTimestamp::IceTimestamp(int64_t log_time_us, std::size_t log_id)
    : log_time_us_(log_time_us), log_id_(log_id) {
  // TODO(zstein): use constants
  RTC_DCHECK(log_id == 0 || log_id == 1);
}

IceTransaction::IceTransaction() = default;

IceTransaction::ConnectionId IceTransaction::connection_id() const {
  return std::make_pair(log1_candidate_pair_id.value_or(0),
                        log2_candidate_pair_id.value_or(0));
}

int IceTransaction::stage_reached() const {
  if (!ping_sent_) {
    return 0;
  } else if (!ping_received_) {
    return 1;
  } else if (!response_sent_) {
    return 2;
  } else if (!response_received_) {
    return 3;
  }
  return 4;
}

absl::optional<IceTimestamp> IceTransaction::start_time() const {
  return ping_sent_;
}

absl::optional<IceTimestamp> IceTransaction::end_time() const {
  return response_received_;
}

void IceTransaction::Update(const LoggedIceCandidatePairEvent& event,
                            std::size_t log_id) {
  IceTimestamp timestamp(event.timestamp_us, log_id);

  switch (event.type) {
    case IceCandidatePairEventType::kCheckSent:
      RTC_DCHECK(!ping_sent_);
      ping_sent_.emplace(event.timestamp_us, log_id);
      break;
    case IceCandidatePairEventType::kCheckReceived:
      RTC_DCHECK(!ping_received_);
      ping_received_.emplace(event.timestamp_us, log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseSent:
      RTC_DCHECK(!response_sent_);
      response_sent_.emplace(event.timestamp_us, log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseReceived:
      RTC_DCHECK(!response_received_);
      response_received_.emplace(event.timestamp_us, log_id);
      break;
    case IceCandidatePairEventType::kNumValues:
      RTC_NOTREACHED();
      break;
  }
}

int64_t min_timestamp(const std::vector<LoggedIceCandidatePairEvent>& events) {
  return std::min_element(events.begin(), events.end(),
                          [](const LoggedIceCandidatePairEvent& a,
                             const LoggedIceCandidatePairEvent& b) {
                            return a.log_time_us() < b.log_time_us();
                          })
      ->log_time_us();
}

MultiEventLogAnalyzer::MultiEventLogAnalyzer(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    int64_t log1_first_timestamp,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events,
    int64_t log2_first_timestamp)
    : log1_first_timestamp_(log1_first_timestamp),
      log2_first_timestamp_(log2_first_timestamp) {
  log1_min_timestamp_ = min_timestamp(log1_events);
  log2_min_timestamp_ = min_timestamp(log2_events);
  ClockOffsetCalculator clock_offset;
  clock_offset.ProcessLogs(log1_events, log2_events);
  clock_offset_ = clock_offset.CalculateMedian();
  RTC_LOG(LS_INFO) << "clock offset: " << clock_offset_;

  BuildEventsByTransactionId(log1_events, log2_events);
  BuildIceTransactions(log1_events, log2_events);
}

void MultiEventLogAnalyzer::BuildEventsByTransactionId(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  for (const auto& event : log1_events) {
    events_by_transaction_id_[event.transaction_id].emplace_back(event, 0);
  }
  for (const auto& event : log2_events) {
    events_by_transaction_id_[event.transaction_id].emplace_back(event, 1);
  }
}

void MultiEventLogAnalyzer::CreateIceSequenceDiagrams(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;
  std::unordered_map<Plot*, PlotLimits> plot_x_limits;

  for (const auto& transaction_events : events_by_transaction_id_) {
    TransactionId transaction_id = transaction_events.first;
    const SourcedEventVec& sourced_events = transaction_events.second;
    TimeSeries time_series(std::to_string(transaction_id), LineStyle::kLine,
                           PointStyle::kHighlight);
    std::set<CandidatePairId> candidate_pair_ids;
    for (const auto& sourced_event : sourced_events) {
      candidate_pair_ids.insert(sourced_event.event.candidate_pair_id);
      float x = ToCallTimeSec(sourced_event.event.log_time_us(),
                              sourced_event.log_id);
      float y = sourced_event.log_id;
      time_series.points.emplace_back(x, y);
    }
    std::sort(time_series.points.begin(), time_series.points.end(),
              CompareTimeSeriesPoint);

    Plot* plot;
    for (auto candidate_pair_id : candidate_pair_ids) {
      plot = plots[candidate_pair_id];
      if (plot) {
        break;
      }
    }
    if (plot == nullptr) {
      plot = plot_collection->AppendNewPlot();
      // TODO(zstein): This needs to be computed after all transactions have
      // been processed.
      auto candidate_pair_ids_str = JoinCandidatePairIds(candidate_pair_ids);
      plot->SetTitle("IceSequenceDiagram for candidate_pair_ids " +
                     candidate_pair_ids_str);
      plot->SetSuggestedYAxis(0, 1, "Client", 0, 0);
      plot_x_limits[plot];
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
    }

    for (const auto& point : time_series.points) {
      plot_x_limits[plot].Update(point.x);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }
  for (const auto& p : plot_x_limits) {
    auto* plot = p.first;
    const PlotLimits& limits = p.second;
    plot->SetSuggestedXAxis(limits.min(), limits.max(), "Unnormalized Time (s)",
                            0.01, 0.01);
  }
}

void MultiEventLogAnalyzer::BuildIceTransactions(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  for (const auto& event : log1_events) {
    IceTransaction& transaction = ice_transactions_[event.transaction_id];
    transaction.log1_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, 0);
  }
  for (const auto& event : log2_events) {
    IceTransaction& transaction = ice_transactions_[event.transaction_id];
    transaction.log2_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, 1);
  }
}

void MultiEventLogAnalyzer::CreateIceTransactionGraphs(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;
  std::unordered_map<Plot*, PlotLimits> plot_x_limits;

  for (const auto& transaction_events : events_by_transaction_id_) {
    TransactionId transaction_id = transaction_events.first;
    const SourcedEventVec& sourced_events = transaction_events.second;
    TimeSeries time_series(std::to_string(transaction_id), LineStyle::kLine,
                           PointStyle::kHighlight);
    std::set<CandidatePairId> candidate_pair_ids;
    for (const auto& sourced_event : sourced_events) {
      candidate_pair_ids.insert(sourced_event.event.candidate_pair_id);
      float x =
          ToCallTimeSec(sourced_event.event.timestamp_us, sourced_event.log_id);
      float y = static_cast<float>(sourced_event.event.type);
      time_series.points.emplace_back(x, y);
    }
    std::sort(time_series.points.begin(), time_series.points.end(),
              CompareTimeSeriesPoint);

    Plot* plot;
    for (auto candidate_pair_id : candidate_pair_ids) {
      plot = plots[candidate_pair_id];
      if (plot) {
        break;
      }
    }
    if (plot == nullptr) {
      plot = plot_collection->AppendNewPlot();
      // TODO(zstein): This needs to be computed after all transactions have
      // been processed.
      auto candidate_pair_ids_str = JoinCandidatePairIds(candidate_pair_ids);
      plot->SetTitle("IceTransactions for candidate_pair_ids " +
                     candidate_pair_ids_str);
      plot->SetSuggestedYAxis(
          -1, static_cast<float>(IceCandidatePairEventType::kNumValues) + 1,
          "Numeric IceCandidatePairEvent Type", 0, 0);
      plot_x_limits[plot];
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
    }

    for (const auto& point : time_series.points) {
      plot_x_limits[plot].Update(point.x);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }
  for (const auto& p : plot_x_limits) {
    auto* plot = p.first;
    const PlotLimits& limits = p.second;
    plot->SetSuggestedXAxis(limits.min(), limits.max(), "Unnormalized Time (s)",
                            0.01, 0.01);
  }
}

void MultiEventLogAnalyzer::CreateIceTransactionStateGraphs(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  Plot* plot = plot_collection->AppendNewPlot();
  plot->SetTitle("IceTransactionStateReached");
  plot->SetSuggestedYAxis(0, 5, "Stage Reached", 0, 0);
  PlotLimits x_limits;

  for (const auto& connection : connections) {
    TimeSeries series(ConnectionIdString(connection.first), LineStyle::kNone,
                      PointStyle::kHighlight);
    for (const auto& transaction : connection.second) {
      auto start_time = transaction.start_time();
      if (!start_time) {
        continue;
      }
      // TODO(zstein): Reconsider where on the x axis to draw this.
      float x = ToCallTimeSec(start_time->log_time_us(), start_time->log_id());
      x_limits.Update(x);
      float y = transaction.stage_reached();
      series.points.emplace_back(x, y);
    }

    std::sort(series.points.begin(), series.points.end(),
              CompareTimeSeriesPoint);

    plot->AppendTimeSeries(std::move(series));
  }

  plot->SetSuggestedXAxis(x_limits.min(), x_limits.max(),
                          "Unnormalized Time (s)", 0.01, 0.01);
}

void MultiEventLogAnalyzer::CreateIceTransactionRttGraphs(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  for (const auto& connection : connections) {
    TimeSeries series("", LineStyle::kNone, PointStyle::kHighlight);
    PlotLimits x_limits, y_limits;
    for (const auto& transaction : connection.second) {
      auto start_time = transaction.start_time();
      if (!start_time) {
        continue;
      }
      auto end_time = transaction.end_time();
      float x = ToCallTimeSec(start_time->log_time_us(), start_time->log_id());
      x_limits.Update(x);
      RTC_LOG(LS_WARNING) << "Connection(" << connection.first.first << ", "
                          << connection.first.second
                          << "): " << start_time->log_time_us() << ", "
                          << end_time.has_value();
      if (end_time) {
        RTC_LOG(LS_WARNING)
            << "  " << end_time->log_time_us() << "; "
            << end_time->log_time_us() - start_time->log_time_us();
      }
      float y = end_time.has_value()
                    ? end_time->log_time_us() - start_time->log_time_us()
                    : 0;
      y_limits.Update(y);
      series.points.emplace_back(x, y);
    }

    Plot* plot = plot_collection->AppendNewPlot();
    plot->SetTitle("IceTransaction RTT for candidate_pair_ids " +
                   ConnectionIdString(connection.first));
    plot->SetSuggestedXAxis(x_limits.min(), x_limits.max(),
                            "Unnormalized Time (s)", 0.01, 0.01);
    RTC_LOG(LS_WARNING) << "y limits: " << y_limits.min() << "-"
                        << y_limits.max();
    plot->SetSuggestedYAxis(0, y_limits.max(), "RTT (us)", 0.05, 0.05);
    plot->AppendTimeSeries(std::move(series));
  }
}

float MultiEventLogAnalyzer::FirstTimestamp(std::size_t log_id) {
  // TODO(zstein): Use first_timestamp or search for min_element?
  return log_id == 0 ? log1_first_timestamp_ : log2_first_timestamp_;
  // return log_id == 0 ? log1_min_timestamp_ : log2_min_timestamp_;
}

float MultiEventLogAnalyzer::ToCallTimeSec(int64_t timestamp,
                                           std::size_t log_id) {
  // TODO(zstein): Offset moves timestamps from log2 in to log1 time, so always
  // subtract log1 offset here?
  float log_time = timestamp - FirstTimestamp(0);
  if (log_id == 1) {
    log_time -= clock_offset_;
  }
  return log_time / kNumMicrosecsPerSec;
}

}  // namespace webrtc
