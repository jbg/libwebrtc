/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"

#include <string>
#include <utility>

#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {

class OffsetTransactionBuilder final {
 public:
  OffsetTransactionBuilder(
      std::vector<LoggedIceCandidatePairEvent>* initiator_events,
      std::vector<LoggedIceCandidatePairEvent>* responder_events)
      : initiator_events_(initiator_events),
        responder_events_(responder_events) {}

  OffsetTransactionBuilder& set_initiator_events(
      std::vector<LoggedIceCandidatePairEvent>* initiator_events) {
    initiator_events_ = initiator_events;
    return *this;
  }
  OffsetTransactionBuilder& set_responder_events(
      std::vector<LoggedIceCandidatePairEvent>* responder_events) {
    responder_events_ = responder_events;
    return *this;
  }
  OffsetTransactionBuilder& set_delay_until_start(uint64_t delay_until_start) {
    delay_until_start_ = delay_until_start;
    return *this;
  }
  OffsetTransactionBuilder& set_offset(int64_t offset) {
    offset_ = offset;
    return *this;
  }
  OffsetTransactionBuilder& set_transaction_id(uint32_t transaction_id) {
    transaction_id_ = transaction_id;
    return *this;
  }

  void Build() {
    RTC_DCHECK(initiator_events_);
    RTC_DCHECK(responder_events_);
    uint64_t time = 0 + delay_until_start_;
    initiator_events_->emplace_back(time, IceCandidatePairEventType::kCheckSent,
                                    candidate_pair_id_, transaction_id_);
    time += network_delay_initiator_to_responder_;
    RTC_DCHECK(static_cast<int64_t>(time) + offset_ >= 0);
    responder_events_->emplace_back(time + offset_,
                                    IceCandidatePairEventType::kCheckReceived,
                                    candidate_pair_id_, transaction_id_);
    time += processing_delay_;
    responder_events_->emplace_back(
        time + offset_, IceCandidatePairEventType::kCheckResponseSent,
        candidate_pair_id_, transaction_id_);
    time += network_delay_responder_to_initiator_;
    initiator_events_->emplace_back(
        time, IceCandidatePairEventType::kCheckResponseReceived,
        candidate_pair_id_, transaction_id_);
  }

  OffsetTransactionBuilder& NextTransactionId() {
    ++transaction_id_;
    return *this;
  }

  // Note this also swaps the sign of the offset.
  OffsetTransactionBuilder& SwapOutputEvents() {
    std::swap(initiator_events_, responder_events_);
    offset_ = -offset_;
    return *this;
  }

 private:
  std::vector<LoggedIceCandidatePairEvent>* initiator_events_ = nullptr;
  std::vector<LoggedIceCandidatePairEvent>* responder_events_ = nullptr;

  int64_t offset_ = 0;  // responder offset from initator's clock
  uint32_t transaction_id_ = 1;
  uint64_t delay_until_start_ = 1;  // impl assumes no timestamp 0

  // Not yet configurable, but could be.
  uint64_t network_delay_initiator_to_responder_ = 1;
  uint64_t processing_delay_ = 1;  // of responder
  uint64_t network_delay_responder_to_initiator_ = 1;
  uint32_t candidate_pair_id_ = 1;
};

std::string ToString(const std::vector<LoggedIceCandidatePairEvent>& events) {
  rtc::StringBuilder s;
  s << "LoggedIceCandidatePairEvents: [";
  for (const auto& event : events) {
    s << "{" << event.timestamp_us << "," << static_cast<int>(event.type) << ","
      << event.candidate_pair_id << "," << event.transaction_id << "},";
  }
  s << "]";

  return s.str();
}

class ClockOffsetCalculatorTest : public ::testing::Test {
 public:
  ClockOffsetCalculatorTest() : builder_(&log1_events_, &log2_events_) {}

  OffsetTransactionBuilder& builder() { return builder_; }

  void PrintEventsToLog() {
    RTC_LOG(LS_ERROR) << "log1_events_ = " << ToString(log1_events_);
    RTC_LOG(LS_ERROR) << "log2_events_ = " << ToString(log2_events_);
  }

  void ProcessLogs() {
    // TODO(zstein): Update API to not require start time of 0.
    c_.ProcessLogs(log1_events_, log2_events_);
  }

  void ExpectStats(int64_t expected_count,
                   int64_t expected_mean,
                   int64_t expected_median) {
    auto count = c_.full_sequence_count();
    EXPECT_EQ(expected_count, count);
    auto mean = c_.CalculateMean();
    EXPECT_EQ(expected_mean, mean);
    auto median = c_.CalculateMedian();
    EXPECT_EQ(expected_median, median);
  }

 private:
  std::vector<LoggedIceCandidatePairEvent> log1_events_;
  std::vector<LoggedIceCandidatePairEvent> log2_events_;
  OffsetTransactionBuilder builder_;
  ClockOffsetCalculator c_;
};

TEST_F(ClockOffsetCalculatorTest, TestNoEvents) {
  ProcessLogs();
  ExpectStats(0, 0, 0);
}

TEST_F(ClockOffsetCalculatorTest, TestOneEvent) {
  builder().set_offset(1).Build();

  PrintEventsToLog();
  ProcessLogs();
  ExpectStats(1, 1, 1);
}

TEST_F(ClockOffsetCalculatorTest, TestOneEventWithStartDelay) {
  builder().set_delay_until_start(100).set_offset(1).Build();

  PrintEventsToLog();
  ProcessLogs();
  ExpectStats(1, 1, 1);
}

TEST_F(ClockOffsetCalculatorTest, TestMedianWithThreeEvents) {
  builder().set_offset(1).Build();
  builder().NextTransactionId().set_delay_until_start(100).Build();
  builder()
      .NextTransactionId()
      .set_delay_until_start(200)
      .set_offset(4)
      .Build();

  PrintEventsToLog();
  ProcessLogs();
  ExpectStats(3, 2, 1);
}

TEST_F(ClockOffsetCalculatorTest, TestNegativeOffset) {
  builder().set_delay_until_start(100).set_offset(-1).Build();

  PrintEventsToLog();
  ProcessLogs();
  ExpectStats(1, -1, -1);
}

}  // namespace webrtc
