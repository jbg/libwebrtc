/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/metrics/metrics_logger_and_exporter.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_exporter.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Eq;

std::map<std::string, std::string> DefaultMetadata() {
  return std::map<std::string, std::string>{{"key", "value"}};
}

TEST(MetricsLoggerAndExporterTest, LogSingleValueMetricRecordsMetric) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});
  logger.LogSingleValueMetric(
      "metric_name", "test_case_name",
      /*value=*/10, Unit::kTimeMs, ImprovementDirection::kBiggerIsBetter,
      std::map<std::string, std::string>{{"key", "value"}});

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(1lu));
  const Metric& metric = metrics[0];
  EXPECT_THAT(metric.name, Eq("metric_name"));
  EXPECT_THAT(metric.test_case, Eq("test_case_name"));
  EXPECT_THAT(metric.unit, Eq(Unit::kTimeMs));
  EXPECT_THAT(metric.improvement_direction,
              Eq(ImprovementDirection::kBiggerIsBetter));
  EXPECT_THAT(metric.metric_metadata,
              Eq(std::map<std::string, std::string>{{"key", "value"}}));
  ASSERT_THAT(metric.time_series.samples.size(), Eq(1lu));
  EXPECT_THAT(metric.time_series.samples[0].value, Eq(10.0));
  EXPECT_THAT(metric.time_series.samples[0].sample_metadata,
              Eq(std::map<std::string, std::string>{}));
  ASSERT_THAT(metric.stats.mean, absl::optional<double>(10.0));
  ASSERT_THAT(metric.stats.stddev, absl::nullopt);
  ASSERT_THAT(metric.stats.min, absl::optional<double>(10.0));
  ASSERT_THAT(metric.stats.max, absl::optional<double>(10.0));
}

TEST(MetricsLoggerAndExporterTest,
     LogMetricWithSamplesStatsCounterRecordsMetric) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});

  SamplesStatsCounter values;
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 10,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata =
          std::map<std::string, std::string>{{"point_key1", "value1"}}});
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 20,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata =
          std::map<std::string, std::string>{{"point_key2", "value2"}}});
  logger.LogMetric("metric_name", "test_case_name", values, Unit::kTimeMs,
                   ImprovementDirection::kBiggerIsBetter,
                   std::map<std::string, std::string>{{"key", "value"}});

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(1lu));
  const Metric& metric = metrics[0];
  EXPECT_THAT(metric.name, Eq("metric_name"));
  EXPECT_THAT(metric.test_case, Eq("test_case_name"));
  EXPECT_THAT(metric.unit, Eq(Unit::kTimeMs));
  EXPECT_THAT(metric.improvement_direction,
              Eq(ImprovementDirection::kBiggerIsBetter));
  EXPECT_THAT(metric.metric_metadata,
              Eq(std::map<std::string, std::string>{{"key", "value"}}));
  ASSERT_THAT(metric.time_series.samples.size(), Eq(2lu));
  EXPECT_THAT(metric.time_series.samples[0].value, Eq(10.0));
  EXPECT_THAT(metric.time_series.samples[0].sample_metadata,
              Eq(std::map<std::string, std::string>{{"point_key1", "value1"}}));
  EXPECT_THAT(metric.time_series.samples[1].value, Eq(20.0));
  EXPECT_THAT(metric.time_series.samples[1].sample_metadata,
              Eq(std::map<std::string, std::string>{{"point_key2", "value2"}}));
  ASSERT_THAT(metric.stats.mean, absl::optional<double>(15.0));
  ASSERT_THAT(metric.stats.stddev, absl::optional<double>(5.0));
  ASSERT_THAT(metric.stats.min, absl::optional<double>(10.0));
  ASSERT_THAT(metric.stats.max, absl::optional<double>(20.0));
}

TEST(MetricsLoggerAndExporterTest, LogMetricWithStatsRecordsMetric) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});
  Metric::Stats metric_stats{.mean = 15, .stddev = 5, .min = 10, .max = 20};
  logger.LogMetric("metric_name", "test_case_name", metric_stats, Unit::kTimeMs,
                   ImprovementDirection::kBiggerIsBetter,
                   std::map<std::string, std::string>{{"key", "value"}});

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(1lu));
  const Metric& metric = metrics[0];
  EXPECT_THAT(metric.name, Eq("metric_name"));
  EXPECT_THAT(metric.test_case, Eq("test_case_name"));
  EXPECT_THAT(metric.unit, Eq(Unit::kTimeMs));
  EXPECT_THAT(metric.improvement_direction,
              Eq(ImprovementDirection::kBiggerIsBetter));
  EXPECT_THAT(metric.metric_metadata,
              Eq(std::map<std::string, std::string>{{"key", "value"}}));
  ASSERT_THAT(metric.time_series.samples.size(), Eq(0lu));
  ASSERT_THAT(metric.stats.mean, absl::optional<double>(15.0));
  ASSERT_THAT(metric.stats.stddev, absl::optional<double>(5.0));
  ASSERT_THAT(metric.stats.min, absl::optional<double>(10.0));
  ASSERT_THAT(metric.stats.max, absl::optional<double>(20.0));
}

TEST(MetricsLoggerAndExporterTest, LogSingleValueMetricRecordsMultipleMetrics) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});

  logger.LogSingleValueMetric("metric_name1", "test_case_name1",
                              /*value=*/10, Unit::kTimeMs,
                              ImprovementDirection::kBiggerIsBetter,
                              DefaultMetadata());
  logger.LogSingleValueMetric("metric_name2", "test_case_name2",
                              /*value=*/10, Unit::kTimeMs,
                              ImprovementDirection::kBiggerIsBetter,
                              DefaultMetadata());

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(2lu));
  EXPECT_THAT(metrics[0].name, Eq("metric_name1"));
  EXPECT_THAT(metrics[0].test_case, Eq("test_case_name1"));
  EXPECT_THAT(metrics[1].name, Eq("metric_name2"));
  EXPECT_THAT(metrics[1].test_case, Eq("test_case_name2"));
}

TEST(MetricsLoggerAndExporterTest,
     LogMetricWithSamplesStatsCounterRecordsMultipleMetrics) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});
  SamplesStatsCounter values;
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 10,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata = DefaultMetadata()});
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 20,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata = DefaultMetadata()});

  logger.LogMetric("metric_name1", "test_case_name1", values, Unit::kTimeMs,
                   ImprovementDirection::kBiggerIsBetter, DefaultMetadata());
  logger.LogMetric("metric_name2", "test_case_name2", values, Unit::kTimeMs,
                   ImprovementDirection::kBiggerIsBetter, DefaultMetadata());

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(2lu));
  EXPECT_THAT(metrics[0].name, Eq("metric_name1"));
  EXPECT_THAT(metrics[0].test_case, Eq("test_case_name1"));
  EXPECT_THAT(metrics[1].name, Eq("metric_name2"));
  EXPECT_THAT(metrics[1].test_case, Eq("test_case_name2"));
}

TEST(MetricsLoggerAndExporterTest, LogMetricWithStatsRecordsMultipleMetrics) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});
  Metric::Stats metric_stats{.mean = 15, .stddev = 5, .min = 10, .max = 20};

  logger.LogMetric("metric_name1", "test_case_name1", metric_stats,
                   Unit::kTimeMs, ImprovementDirection::kBiggerIsBetter,
                   DefaultMetadata());
  logger.LogMetric("metric_name2", "test_case_name2", metric_stats,
                   Unit::kTimeMs, ImprovementDirection::kBiggerIsBetter,
                   DefaultMetadata());

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(2lu));
  EXPECT_THAT(metrics[0].name, Eq("metric_name1"));
  EXPECT_THAT(metrics[0].test_case, Eq("test_case_name1"));
  EXPECT_THAT(metrics[1].name, Eq("metric_name2"));
  EXPECT_THAT(metrics[1].test_case, Eq("test_case_name2"));
}

TEST(MetricsLoggerAndExporterTest,
     LogMetricThroughtAllMethodsAccumulateAllMetrics) {
  MetricsLoggerAndExporter logger(
      Clock::GetRealTimeClock(),
      std::vector<std::unique_ptr<MetricsExporter>>{});
  SamplesStatsCounter values;
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 10,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata = DefaultMetadata()});
  values.AddSample(SamplesStatsCounter::StatsSample{
      .value = 20,
      .time = Clock::GetRealTimeClock()->CurrentTime(),
      .metadata = DefaultMetadata()});
  Metric::Stats metric_stats{.mean = 15, .stddev = 5, .min = 10, .max = 20};

  logger.LogSingleValueMetric("metric_name1", "test_case_name1",
                              /*value=*/10, Unit::kTimeMs,
                              ImprovementDirection::kBiggerIsBetter,
                              DefaultMetadata());
  logger.LogMetric("metric_name2", "test_case_name2", values, Unit::kTimeMs,
                   ImprovementDirection::kBiggerIsBetter, DefaultMetadata());
  logger.LogMetric("metric_name3", "test_case_name3", metric_stats,
                   Unit::kTimeMs, ImprovementDirection::kBiggerIsBetter,
                   DefaultMetadata());

  std::vector<Metric> metrics = logger.GetCollectedMetrics();
  ASSERT_THAT(metrics.size(), Eq(3lu));
  EXPECT_THAT(metrics[0].name, Eq("metric_name1"));
  EXPECT_THAT(metrics[0].test_case, Eq("test_case_name1"));
  EXPECT_THAT(metrics[1].name, Eq("metric_name2"));
  EXPECT_THAT(metrics[1].test_case, Eq("test_case_name2"));
  EXPECT_THAT(metrics[2].name, Eq("metric_name3"));
  EXPECT_THAT(metrics[2].test_case, Eq("test_case_name3"));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
