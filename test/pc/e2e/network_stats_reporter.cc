/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/network_stats_reporter.h"

#include <utility>

#include "rtc_base/event.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kStatsWaitTimeoutMs = 1000;

}

void NetworkStatsReporter::Start(std::string test_case_name) {
  test_case_name_ = std::move(test_case_name);
  // Check that network stats are clean before test execution.
  EmulatedNetworkStats alice_stats = PopulateStats(alice_network_);
  RTC_CHECK_EQ(alice_stats.packets_sent, 0);
  RTC_CHECK_EQ(alice_stats.packets_received, 0);
  EmulatedNetworkStats bob_stats = PopulateStats(bob_network_);
  RTC_CHECK_EQ(bob_stats.packets_sent, 0);
  RTC_CHECK_EQ(bob_stats.packets_received, 0);
}

void NetworkStatsReporter::Stop() {
  EmulatedNetworkStats alice_stats = PopulateStats(alice_network_);
  EmulatedNetworkStats bob_stats = PopulateStats(bob_network_);
  ReportStats("alice", alice_stats,
              alice_stats.packets_sent - bob_stats.packets_received);
  ReportStats("bob", alice_stats,
              bob_stats.packets_sent - alice_stats.packets_received);
}

EmulatedNetworkStats NetworkStatsReporter::PopulateStats(
    EmulatedNetworkManagerInterface* network) {
  rtc::Event wait;
  EmulatedNetworkStats stats;
  network->GetStats([&](const EmulatedNetworkStats& s) {
    stats = s;
    wait.Set();
  });
  bool stats_received = wait.Wait(kStatsWaitTimeoutMs);
  RTC_CHECK(stats_received);
  return stats;
}

void NetworkStatsReporter::ReportStats(const std::string& network_label,
                                       const EmulatedNetworkStats& stats,
                                       int64_t packet_loss) {
  ReportResult("bytes_sent", network_label, stats.bytes_sent.bytes(),
               "sizeInBytes");
  ReportResult("packets_sent", network_label, stats.packets_sent, "unitless");
  ReportResult("average_send_rate", network_label,
               stats.AverageSendRate().bytes_per_sec(), "bytesPerSecond");
  ReportResult("bytes_received", network_label, stats.bytes_received.bytes(),
               "sizeInBytes");
  ReportResult("packets_received", network_label, stats.packets_received,
               "unitless");
  ReportResult("average_receive_rate", network_label,
               stats.AverageReceiveRate().bytes_per_sec(), "bytesPerSecond");
  ReportResult("packets_loss", network_label, packet_loss, "unitless");
}

void NetworkStatsReporter::ReportResult(const std::string& metric_name,
                                        const std::string& network_label,
                                        const double value,
                                        const std::string& unit) const {
  test::PrintResult(metric_name, /*modifier=*/"",
                    GetTestCaseName(network_label), value, unit,
                    /*important=*/false);
}

std::string NetworkStatsReporter::GetTestCaseName(
    const std::string& network_label) const {
  return test_case_name_ + "/" + network_label;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
