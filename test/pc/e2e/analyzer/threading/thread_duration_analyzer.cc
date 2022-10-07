/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/threading/thread_duration_analyzer.h"

#include "absl/strings/string_view.h"
#include "api/test/metrics/metric.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

void ThreadDurationAnalyzer::OnLatencyMeasured(std::string name,
                                               TimeDelta latency) {
  MutexLock lock(&mutex_);
  latency_map_[name].AddSample(latency.ms<double>());
}

void ThreadDurationAnalyzer::OnTaskDurationMeasured(std::string name,
                                                    TimeDelta duration) {
  MutexLock lock(&mutex_);
  duration_map_[name].AddSample(duration.ms<double>());
}

void ThreadDurationAnalyzer::LogMetrics(test::MetricsLogger& metrics_logger,
                                        absl::string_view test_name) {
  MutexLock lock(&mutex_);
  for (const auto& [name, data] : latency_map_) {
    rtc::StringBuilder ss;
    ss << "TaskQueue[" << name << "].Latency";
    metrics_logger.LogMetric(ss.Release(), test_name, data,
                             test::Unit::kMilliseconds,
                             test::ImprovementDirection::kSmallerIsBetter);
  }
  for (const auto& [name, data] : duration_map_) {
    rtc::StringBuilder ss;
    ss << "TaskQueue[" << name << "].Duration";
    metrics_logger.LogMetric(ss.Release(), test_name, data,
                             test::Unit::kMilliseconds,
                             test::ImprovementDirection::kSmallerIsBetter);
  }
}

}  // namespace webrtc
