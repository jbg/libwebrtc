/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/testsupport/metrics/metrics_logger_and_exporter.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/testsupport/metrics/metric.h"

namespace webrtc {
namespace test {

void MetricsLoggerAndExporter::LogSingleValueMetric(
    absl::string_view name,
    absl::string_view test_case_name,
    double value,
    Unit unit,
    ImprovementDirection improvement_direction,
    std::map<std::string, std::string> metadata) {
  MutexLock lock(&mutex_);
  metrics_.push_back(Metric{
      .name = std::string(name),
      .unit = unit,
      .improvement_direction = improvement_direction,
      .test_case = std::string(test_case_name),
      .metadata = std::move(metadata),
      .time_series =
          Metric::TimeSeries{.values =
                                 std::vector{Metric::TimeSeries::DataPoint{
                                     .timestamp = Now(), .value = value}}},
      .stats = Metric::Stats{
          .mean = value, .stddev = absl::nullopt, .min = value, .max = value}});
}

void MetricsLoggerAndExporter::LogMetric(
    absl::string_view name,
    absl::string_view test_case_name,
    const SamplesStatsCounter& values,
    Unit unit,
    ImprovementDirection improvement_direction,
    std::map<std::string, std::string> metadata) {
  MutexLock lock(&mutex_);
  Metric::TimeSeries time_series;
  for (const SamplesStatsCounter::StatsSample& sample :
       values.GetTimedSamples()) {
    time_series.values.push_back(Metric::TimeSeries::DataPoint{
        .timestamp = sample.time, .value = sample.value, .metadata = metadata});
  }

  metrics_.push_back(
      Metric{.name = std::string(name),
             .unit = unit,
             .improvement_direction = improvement_direction,
             .test_case = std::string(test_case_name),
             .metadata = std::move(metadata),
             .time_series = std::move(time_series),
             .stats = Metric::Stats{.mean = values.GetAverage(),
                                    .stddev = values.GetStandardDeviation(),
                                    .min = values.GetMin(),
                                    .max = values.GetMax()}});
}

void MetricsLoggerAndExporter::LogMetric(
    absl::string_view name,
    absl::string_view test_case_name,
    Metric::Stats metric_stats,
    Unit unit,
    ImprovementDirection improvement_direction,
    std::map<std::string, std::string> metadata) {
  MutexLock lock(&mutex_);
  metrics_.push_back(Metric{.name = std::string(name),
                            .unit = unit,
                            .improvement_direction = improvement_direction,
                            .test_case = std::string(test_case_name),
                            .metadata = std::move(metadata),
                            .time_series = Metric::TimeSeries{.values = {}},
                            .stats = std::move(metric_stats)});
}

Timestamp MetricsLoggerAndExporter::Now() {
  return clock_->CurrentTime();
}

}  // namespace test
}  // namespace webrtc
