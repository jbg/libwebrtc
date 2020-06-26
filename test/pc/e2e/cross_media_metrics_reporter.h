/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_CROSS_MEDIA_METRICS_REPORTER_H_
#define TEST_PC_E2E_CROSS_MEDIA_METRICS_REPORTER_H_

#include <map>
#include <string>

#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/units/timestamp.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class CrossMediaMetricsReporter : public StatsObserverInterface {
 public:
  CrossMediaMetricsReporter() = default;
  ~CrossMediaMetricsReporter() override = default;

  void Start(absl::string_view test_case_name,
             TrackIdStreamLabelMap* analyzer_helper);
  void OnStatsReports(
      absl::string_view pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) override;
  void Stop();

 private:
  struct Stats {
    SamplesStatsCounter audio_ahead_ms;
    SamplesStatsCounter video_ahead_ms;
  };

  struct AVSyncGroupKey {
    Timestamp timestamp;
    absl::string_view stream_label;

    bool operator<(const AVSyncGroupKey& other) const {
      if (this->timestamp != other.timestamp) {
        return this->timestamp < other.timestamp;
      }
      return this->stream_label < other.stream_label;
    }
  };

  static void ReportResult(const std::string& metric_name,
                           const std::string& test_case_name,
                           const SamplesStatsCounter& counter,
                           const std::string& unit,
                           webrtc::test::ImproveDirection improve_direction =
                               webrtc::test::ImproveDirection::kNone);
  std::string GetTestCaseName(const absl::string_view stream_label) const;

  std::string test_case_name_;
  TrackIdStreamLabelMap* analyzer_helper_;

  rtc::CriticalSection lock_;
  std::map<absl::string_view, Stats> stats_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_CROSS_MEDIA_METRICS_REPORTER_H_
