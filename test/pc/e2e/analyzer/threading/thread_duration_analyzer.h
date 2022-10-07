/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_THREADING_THREAD_DURATION_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_THREADING_THREAD_DURATION_ANALYZER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/units/time_delta.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
namespace webrtc {

class ThreadDurationAnalyzer : public rtc::RefCountInterface {
 public:
  void OnLatencyMeasured(std::string name, TimeDelta latency);
  void OnTaskDurationMeasured(std::string name, TimeDelta duration);

  void LogMetrics(test::MetricsLogger& metrics_logger,
                  absl::string_view test_name);

 private:
  Mutex mutex_;
  flat_map<std::string, SamplesStatsCounter> latency_map_;
  flat_map<std::string, SamplesStatsCounter> duration_map_;
};
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_THREADING_THREAD_DURATION_ANALYZER_H_
