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

#include "rtc_tools/event_log_visualizer/plot_protobuf.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void ExpectFloatVecEq(std::vector<float> actual, std::vector<float> expected) {
  // Note that "EXPECT_EQ(actual, expected);" gives easier to parse output when
  // debugging (but doesn't do approximate comparison).
  EXPECT_EQ(actual.size(), expected.size());
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_NEAR(actual[i], expected[i], 0.00001);
  }
}

}  // namespace

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation) {
  // first_timestamp is 100 000 000
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {100100000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {100100300, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // first_timestamp is 200 000 000
  // offset should be 50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {200100150, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {200100250, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};
  // TODO(zstein): Repeat limits less.
  MultiEventLogAnalyzerConfig config(log1_events, 100000000, 100000300,
                                     log2_events, 200000200);
  MultiEventLogAnalyzer analyzer(config, log1_events, 100000000, log2_events,
                                 200000000);

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
  std::vector<float> expected{0.1, 0.1001, 0.1002, 0.1003};
  ExpectFloatVecEq(actual, expected);
}

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation2) {
  // first_timestamp is 200 000 000
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {200100000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {200100300, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // first_timestamp is 100 000 000
  // offset should be 50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {100100150, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {100100250, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};
  MultiEventLogAnalyzerConfig config(log1_events, 200000000, 200100300,
                                     log2_events, 100100250);
  MultiEventLogAnalyzer analyzer(config, log1_events, 200000000, log2_events,
                                 100000000);

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
  std::vector<float> expected{0.1, 0.1001, 0.1002, 0.1003};
  ExpectFloatVecEq(actual, expected);
}

TEST(MultiEventLogAnalyzerTest, TestTimestampTranslation3) {
  // first_timestamp is 100 000 000
  std::vector<LoggedIceCandidatePairEvent> log1_events{
      {100100000, IceCandidatePairEventType::kCheckSent, 1, 1},
      {100100300, IceCandidatePairEventType::kCheckResponseReceived, 1, 1}};
  // first_timestamp is 200 000 000
  // offset should be -50
  std::vector<LoggedIceCandidatePairEvent> log2_events{
      {200100050, IceCandidatePairEventType::kCheckReceived, 1, 1},
      {200100150, IceCandidatePairEventType::kCheckResponseSent, 1, 1}};

  MultiEventLogAnalyzerConfig config(log1_events, 100100000, 100100300,
                                     log2_events, 200100150);

  MultiEventLogAnalyzer analyzer(config, log1_events, 100000000, log2_events,
                                 200000000);

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
  std::vector<float> expected{0.1, 0.1001, 0.1002, 0.1003};
  ExpectFloatVecEq(actual, expected);
}

}  // namespace webrtc
