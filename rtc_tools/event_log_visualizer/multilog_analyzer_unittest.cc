/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/multilog_analyzer.h"

#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"
#include "rtc_tools/event_log_visualizer/plot_protobuf.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void ExpectFloatVecEq(std::vector<float> actual, std::vector<float> expected) {
  // Note that "EXPECT_EQ(actual, expected);" gives easier to parse output when
  // debugging (but doesn't do approximate comparison).
  EXPECT_EQ(actual, expected);
  /*/
  EXPECT_EQ(actual.size(), expected.size());
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_NEAR(actual[i], expected[i], 0.00001);
  }
  //*/
}

}  // namespace

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation) {
  int64_t log1_first_timestamp_ms = 100000000;
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {100100000000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {100100300000, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // offset should be 50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {200100150000, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {200100250000, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};

  MultiEventLogAnalyzerConfig config;
  config.log1_first_timestamp_ms = log1_first_timestamp_ms;
  ClockOffsetCalculator c;
  c.ProcessLogs(log1_events, log2_events);
  config.clock_offset_ms = c.CalculateMedianOffsetMs();
  MultiEventLogAnalyzer analyzer(config, log1_events, log2_events);

  ProtobufPlotCollection plot_collection;
  analyzer.CreateIceTransactionGraphs(&plot_collection);

  webrtc::analytics::ChartCollection protobuf_charts;
  plot_collection.ExportProtobuf(&protobuf_charts);

  ASSERT_EQ(1, protobuf_charts.charts_size());
  auto chart = protobuf_charts.charts(0);
  ASSERT_EQ(1, chart.data_sets_size());
  auto data_set = chart.data_sets(0);
  ASSERT_EQ(4, data_set.x_values_size());
  std::vector<float> actual(data_set.x_values().begin(),
                            data_set.x_values().end());
  std::vector<float> expected{100.0, 100.1, 100.2, 100.3};
  ExpectFloatVecEq(actual, expected);
}

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation2) {
  int64_t log1_first_timestamp_ms = 200000000;
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {200100000000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {200100300000, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // offset should be 50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {100100150000, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {100100250000, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};

  MultiEventLogAnalyzerConfig config;
  config.log1_first_timestamp_ms = log1_first_timestamp_ms;
  ClockOffsetCalculator c;
  c.ProcessLogs(log1_events, log2_events);
  config.clock_offset_ms = c.CalculateMedianOffsetMs();
  MultiEventLogAnalyzer analyzer(config, log1_events, log2_events);

  ProtobufPlotCollection plot_collection;
  analyzer.CreateIceTransactionGraphs(&plot_collection);

  webrtc::analytics::ChartCollection protobuf_charts;
  plot_collection.ExportProtobuf(&protobuf_charts);

  ASSERT_EQ(1, protobuf_charts.charts_size());
  auto chart = protobuf_charts.charts(0);
  ASSERT_EQ(1, chart.data_sets_size());
  auto data_set = chart.data_sets(0);
  ASSERT_EQ(4, data_set.x_values_size());
  std::vector<float> actual(data_set.x_values().begin(),
                            data_set.x_values().end());
  std::vector<float> expected{100.0, 100.1, 100.2, 100.3};
  ExpectFloatVecEq(actual, expected);
}

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation3) {
  int64_t log1_first_timestamp_ms = 100000000;
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {100100000000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {100100300000, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // offset should be -50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {200100050000, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {200100150000, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};

  MultiEventLogAnalyzerConfig config;
  config.log1_first_timestamp_ms = log1_first_timestamp_ms;
  ClockOffsetCalculator c;
  c.ProcessLogs(log1_events, log2_events);
  config.clock_offset_ms = c.CalculateMedianOffsetMs();
  MultiEventLogAnalyzer analyzer(config, log1_events, log2_events);

  ProtobufPlotCollection plot_collection;
  analyzer.CreateIceTransactionGraphs(&plot_collection);

  webrtc::analytics::ChartCollection protobuf_charts;
  plot_collection.ExportProtobuf(&protobuf_charts);

  ASSERT_EQ(1, protobuf_charts.charts_size());
  auto chart = protobuf_charts.charts(0);
  ASSERT_EQ(1, chart.data_sets_size());
  auto data_set = chart.data_sets(0);
  ASSERT_EQ(4, data_set.x_values_size());
  std::vector<float> actual(data_set.x_values().begin(),
                            data_set.x_values().end());
  std::vector<float> expected{100.0, 100.1, 100.2, 100.3};
  ExpectFloatVecEq(actual, expected);
}

// TODO(zstein): x axis limits tests

}  // namespace webrtc
