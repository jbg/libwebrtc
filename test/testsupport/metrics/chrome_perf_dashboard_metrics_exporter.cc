/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/testsupport/metrics/chrome_perf_dashboard_metrics_exporter.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/metrics/metric.h"
#include "test/testsupport/perf_test_histogram_writer.h"
#include "test/testsupport/perf_test_result_writer.h"

namespace webrtc {
namespace test {
namespace {

struct ChromePerfDashboardUnit {
  std::string unit;
  double updated_value;
};

ChromePerfDashboardUnit ToChromePerfDashboardUnit(double value, Unit unit) {
  switch (unit) {
    case Unit::kTimeMs:
      return ChromePerfDashboardUnit{.unit = "msBestFitFormat",
                                     .updated_value = value};
    case Unit::kPercent:
      return ChromePerfDashboardUnit{.unit = "n%", .updated_value = value};
    case Unit::kSizeInBytes:
      return ChromePerfDashboardUnit{.unit = "sizeInBytes",
                                     .updated_value = value};
    case Unit::kKilobitsPerSecond:
      return ChromePerfDashboardUnit{.unit = "bytesPerSecond",
                                     .updated_value = value * 1000 / 8};
    case Unit::kHertz:
      return ChromePerfDashboardUnit{.unit = "Hz", .updated_value = value};
    case Unit::kUnitless:
      return ChromePerfDashboardUnit{.unit = "unitless",
                                     .updated_value = value};
    case Unit::kCount:
      return ChromePerfDashboardUnit{.unit = "count", .updated_value = value};
  }
}

ImproveDirection ToChromePerfDashboardImproveDirection(
    ImprovementDirection direction) {
  switch (direction) {
    case ImprovementDirection::kBiggerIsBetter:
      return ImproveDirection::kBiggerIsBetter;
    case ImprovementDirection::kBothPossible:
      return ImproveDirection::kNone;
    case ImprovementDirection::kSmallerIsBetter:
      return ImproveDirection::kSmallerIsBetter;
  }
}

bool WriteMetricsToTheFile(const std::string& path, const std::string& data) {
  CreateDir(DirName(path));
  FILE* output = fopen(path.c_str(), "wb");
  if (output == NULL) {
    printf("Failed to write to %s.\n", path.c_str());
    return false;
  }
  size_t written = fwrite(data.c_str(), sizeof(char), data.size(), output);
  fclose(output);

  if (written != data.size()) {
    size_t expected = data.size();
    printf("Wrote %zu, tried to write %zu\n", written, expected);
    return false;
  }
  return true;
}

}  // namespace

ChromePerfDashboardMetricsExporter::ChromePerfDashboardMetricsExporter(
    absl::string_view export_file_path)
    : export_file_path_(export_file_path) {}

bool ChromePerfDashboardMetricsExporter::Export(
    rtc::ArrayView<const Metric> metrics) {
  std::unique_ptr<PerfTestResultWriter> writer =
      absl::WrapUnique<PerfTestResultWriter>(CreateHistogramWriter());
  for (const Metric& metric : metrics) {
    ChromePerfDashboardUnit unit;
    std::vector<double> samples(metric.time_series.values.size());
    for (size_t i = 0; i < metric.time_series.values.size(); ++i) {
      unit = ToChromePerfDashboardUnit(metric.time_series.values[i].value,
                                       metric.unit);
      samples[i] = unit.updated_value;
    }
    // If we have an empty counter, default it to 0.
    if (samples.empty()) {
      unit = ToChromePerfDashboardUnit(0, metric.unit);
      samples.push_back(unit.updated_value);
    }

    writer->LogResultList(
        metric.name, metric.test_case, samples, unit.unit,
        /*important=*/false,
        ToChromePerfDashboardImproveDirection(metric.improvement_direction));
  }
  return WriteMetricsToTheFile(export_file_path_, writer->Serialize());
}

}  // namespace test
}  // namespace webrtc
