/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/prioritybasedallocationstrategy.h"
#include "rtc_base/gunit.h"

namespace rtc {

// TODO(shampson): Write test for allocating w/o available bitrate for min.
// TODO(shampson): Clean up tests & consider more edge cases

TEST(PriorityBasedAllocationStrategyTest, MinAllocated) {
  std::map<std::string, double> track_priority_map;
  track_priority_map["low"] =  2.0;
  track_priority_map["med"] =  4.0;
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(6000, 10000, false, "low"),
      BitrateAllocationStrategy::TrackConfig(30000, 40000, false, "med")};
  PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
  std::vector<const BitrateAllocationStrategy::TrackConfig*>
      track_config_ptrs(track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }

  std::vector<uint32_t> allocations =
      allocation_strategy.AllocateBitrates(36000, track_config_ptrs);

  EXPECT_EQ(6000u, allocations[0]);
  EXPECT_EQ(30000u, allocations[1]);
}

TEST(PriorityBasedAllocationStrategyTest, OneStreamsBasic) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(0, 2000, false, "low")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(1000, track_config_ptrs);

    EXPECT_EQ(1000u, allocations[0]);
  }

TEST(PriorityBasedAllocationStrategyTest, TwoStreamsBasic) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(0, 2000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(0, 4000, false, "med")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(3000, track_config_ptrs);

    EXPECT_EQ(1000u, allocations[0]);
    EXPECT_EQ(2000u, allocations[1]);
  }

  TEST(PriorityBasedAllocationStrategyTest, TwoStreamsBothAllocatedAboveMin) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(1000, 3000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(2000, 5000, false, "med")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(6000, track_config_ptrs);

    EXPECT_EQ(2000u, allocations[0]);
    EXPECT_EQ(4000u, allocations[1]);
  }

  TEST(PriorityBasedAllocationStrategyTest, TwoStreamsOneAllocatedToMax) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(1000, 4000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(1000, 3000, false, "med")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(6000, track_config_ptrs);

    EXPECT_EQ(3000u, allocations[0]);
    EXPECT_EQ(3000u, allocations[1]);
  }

  TEST(PriorityBasedAllocationStrategyTest, ThreeStreamsOneAllocatedToMax) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    track_priority_map["high"] = 8.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(1000, 3000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(1000, 6000, false, "med"),
        BitrateAllocationStrategy::TrackConfig(1000, 4000, false, "high")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(9000, track_config_ptrs);

    EXPECT_EQ(2000u, allocations[0]);
    EXPECT_EQ(3000u, allocations[1]);
    EXPECT_EQ(4000u, allocations[2]);
  }

  TEST(PriorityBasedAllocationStrategyTest, FourStreamsBasicAllocation) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["very_low"] = 1.0;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    track_priority_map["high"] = 8.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(0, 3000, false, "very_low"),
        BitrateAllocationStrategy::TrackConfig(0, 3000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(0, 6000, false, "med"),
        BitrateAllocationStrategy::TrackConfig(0, 10000, false, "high")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(15000, track_config_ptrs);

    EXPECT_EQ(1000u, allocations[0]);
    EXPECT_EQ(2000u, allocations[1]);
    EXPECT_EQ(4000u, allocations[2]);
    EXPECT_EQ(8000u, allocations[3]);
  }

  TEST(PriorityBasedAllocationStrategyTest, MaxAllocated) {
    std::map<std::string, double> track_priority_map;
    track_priority_map["low"] =  2.0;
    track_priority_map["med"] =  4.0;
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
        BitrateAllocationStrategy::TrackConfig(6000, 10000, false, "low"),
        BitrateAllocationStrategy::TrackConfig(30000, 40000, false, "med")};
    PriorityBasedAllocationStrategy allocation_strategy(track_priority_map);
    std::vector<const BitrateAllocationStrategy::TrackConfig*>
        track_config_ptrs(track_configs.size());
    int i = 0;
    for (const auto& c : track_configs) {
      track_config_ptrs[i++] = &c;
    }

    std::vector<uint32_t> allocations =
        allocation_strategy.AllocateBitrates(60000, track_config_ptrs);

    EXPECT_EQ(10000u, allocations[0]);
    EXPECT_EQ(40000u, allocations[1]);
  }

}  // namespace rtc
