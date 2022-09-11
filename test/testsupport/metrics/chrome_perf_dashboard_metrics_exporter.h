/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_METRICS_CHROME_PERF_DASHBOARD_METRICS_EXPORTER_H_
#define TEST_TESTSUPPORT_METRICS_CHROME_PERF_DASHBOARD_METRICS_EXPORTER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "test/testsupport/metrics/metric.h"
#include "test/testsupport/metrics/metrics_exporter.h"

namespace webrtc {
namespace test {

// Exports all collected metrics in the Chrome Perf Dashboard proto format into
// binary proto file.
class ChromePerfDashboardMetricsExporter : public MetricsExporter {
 public:
  // `export_file_path` - file to export proto.
  explicit ChromePerfDashboardMetricsExporter(
      absl::string_view export_file_path);
  ~ChromePerfDashboardMetricsExporter() override = default;

  bool Export(rtc::ArrayView<const Metric> metrics) override;

 private:
  const std::string export_file_path_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_METRICS_CHROME_PERF_DASHBOARD_METRICS_EXPORTER_H_
