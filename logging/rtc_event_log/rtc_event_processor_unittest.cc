/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_processor.h"

#include <initializer_list>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
std::vector<LoggedStartEvent> CreateEventList(std::initializer_list<int> list) {
  std::vector<LoggedStartEvent> v;
  for (int elem : list) {
    v.emplace_back(elem * 1000);  // Convert ms to us.
  }
  return v;
}

std::vector<std::vector<LoggedStartEvent>>
CreateRandomEventLists(size_t num_lists, size_t num_elements, uint64_t seed) {
  Random prng(seed);
  std::vector<std::vector<LoggedStartEvent>> lists(num_lists);

  for (size_t elem = 0; elem < num_elements; elem++) {
    int i = prng.Rand(0, num_lists - 1);
    lists[i].emplace_back(elem * 1000);
  }
  return lists;
}
}  // namespace

TEST(RtcEventProcessor, NoList) {
  RtcEventProcessor processor;
  processor.ProcessEventsInOrder();  // Don't crash but do nothing.
}

TEST(RtcEventProcessor, EmptyList) {
  auto not_called = [](LoggedStartEvent /*elem*/) { RTC_NOTREACHED(); };
  std::vector<LoggedStartEvent> v;
  RtcEventProcessor processor;

  processor.AddEvents(absl::make_unique<ProcessableEventList<LoggedStartEvent>>(
      v.begin(), v.end(), not_called));
  processor.ProcessEventsInOrder();  // Don't crash but do nothing.
}

TEST(RtcEventProcessor, OneList) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  std::vector<LoggedStartEvent> v(CreateEventList({1, 2, 3, 4}));
  RtcEventProcessor processor;
  processor.AddEvents(absl::make_unique<ProcessableEventList<LoggedStartEvent>>(
      v.begin(), v.end(), f));
  processor.ProcessEventsInOrder();

  ASSERT_EQ(result.size(), 4u);
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(result[i].log_time_ms(), i + 1);
  }
}

TEST(RtcEventProcessor, MergeTwoLists) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  std::vector<LoggedStartEvent> v1(CreateEventList({1, 2, 4, 7, 8, 9}));
  std::vector<LoggedStartEvent> v2(CreateEventList({3, 5, 6, 10}));
  RtcEventProcessor processor;
  processor.AddEvents(absl::make_unique<ProcessableEventList<LoggedStartEvent>>(
      v1.begin(), v1.end(), f));
  processor.AddEvents(absl::make_unique<ProcessableEventList<LoggedStartEvent>>(
      v2.begin(), v2.end(), f));
  processor.ProcessEventsInOrder();

  ASSERT_EQ(result.size(), 10u);
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(result[i].log_time_ms(), i + 1);
  }
}

TEST(RtcEventProcessor, MergeManyLists) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  constexpr size_t kNumLists = 5;
  constexpr size_t kNumElems = 30;
  constexpr uint64_t kSeed = 0xF3C6B91F;
  std::vector<std::vector<LoggedStartEvent>> lists(
      CreateRandomEventLists(kNumLists, kNumElems, kSeed));
  RTC_DCHECK_EQ(lists.size(), kNumLists);
  RtcEventProcessor processor;
  for (const auto& list : lists) {
    processor.AddEvents(
        absl::make_unique<ProcessableEventList<LoggedStartEvent>>(
            list.begin(), list.end(), f));
  }
  processor.ProcessEventsInOrder();

  ASSERT_EQ(result.size(), kNumElems);
  for (size_t i = 0; i < kNumElems; i++) {
    EXPECT_EQ(result[i].log_time_ms(), static_cast<int>(i));
  }
}

}  // namespace webrtc
