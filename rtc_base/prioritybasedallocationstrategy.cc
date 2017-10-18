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

#include <utility>

#include "rtc_base/checks.h"

namespace rtc {

PriorityBasedAllocationStrategy::PriorityBasedAllocationStrategy(
    std::map<std::string, double> track_priority_map)
    : track_priority_map_(track_priority_map) {}

std::vector<uint32_t> PriorityBasedAllocationStrategy::AllocateBitrates(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*> track_configs) {
   RTC_DCHECK_EQ(track_configs.size(), track_priority_map_.size());
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto*& track_config : track_configs) {
    RTC_DCHECK_NE(track_config->track_id, "");
    sum_min_bitrates += track_config->min_bitrate_bps;
    sum_max_bitrates += track_config->max_bitrate_bps;
  }

  if (available_bitrate <= sum_min_bitrates)
    return LowRateAllocationByPriority(available_bitrate, track_configs);

  if (available_bitrate < sum_max_bitrates)
    return NormalRateAllocationByPriority(available_bitrate, track_configs);

  return MaxRateAllocation(available_bitrate, track_configs);

}

std::vector<uint32_t> PriorityBasedAllocationStrategy::LowRateAllocationByPriority(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*> track_configs) {
  std::vector<uint32_t> track_allocations;
  int64_t remaining_bitrate = available_bitrate;

  // First allocate to the tracks that enforce it.
  for (const auto* track_config : track_configs) {
    int32_t allocated_bitrate = 0;
    if (track_config->enforce_min_bitrate) {
      allocated_bitrate = track_config->min_bitrate_bps;
    }
    track_allocations.push_back(allocated_bitrate);
    remaining_bitrate -= allocated_bitrate;
  }

  // Next allocate to all other tracks if there is sufficient bandwidth.
  int i = 0;
  if (remaining_bitrate > 0) {
    // TODO(shampson): Consider allocating min bitrates for the higher priority
    // tracks first.
    for (const auto* track_config : track_configs) {
      if (track_config->enforce_min_bitrate) {
        i++;
        continue;
      }
      if (track_config->min_bitrate_bps <= remaining_bitrate) {
        track_allocations[i++] = track_config->min_bitrate_bps;
        remaining_bitrate -= track_config->min_bitrate_bps;
      }
    }
  }

  // TODO(shampson): Distribute the rest evenly (or based upon priority) to
  // streams that have an allocation.
  return track_allocations;
}

std::vector<uint32_t> PriorityBasedAllocationStrategy::NormalRateAllocationByPriority(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*> track_configs) {
  std::vector<uint32_t> track_allocations;
  int64_t remaining_bitrate = available_bitrate;
  // Pairs of (scaled_track_bandwidth, relative_bitrate) for each track, where
  // scaled_track_bandwidth = (max_bitrate_bps - min_bitrate_bps) / relative_bitrate.
  std::vector<std::pair<double, double>> scaled_track_bandwidths;
  // This is the factor multiplied by a target allocation range to find how much total
  // bitrate will be allocated for that range to the different tracks.
  double track_allocation_factor = 0;

  // Calculate scaled_track_bandwidths & update the remaining bitrate.
  for (const auto* track_config : track_configs) {
    remaining_bitrate -= track_config->min_bitrate_bps;
    // Calculate and store the scaled track bandwidth. This is the track's
    // bandwidth available to be allocated then scaled by it's relative_bitrate.
    double relative_bitrate = track_priority_map_[track_config->track_id];
    uint32_t bandwidth_range =
        track_config->max_bitrate_bps - track_config->min_bitrate_bps;
    uint32_t scaled_bandwidth = static_cast<double>(bandwidth_range) / relative_bitrate;
    scaled_track_bandwidths.push_back(std::pair<double, double>(
        scaled_bandwidth, relative_bitrate));
    // At the start all tracks will get allocated bitrate from remaining
    // bitrate and therefore will contribute to the allocation factor.
    track_allocation_factor += relative_bitrate;
  }

  std::sort(scaled_track_bandwidths.begin(), scaled_track_bandwidths.end());

  // Iterate through the target_allocation_points until we have reached a point
  // that we can no longer allocate bps.
  double last_target_allocation = 0;
  for (const auto& scaled_track_bandwidth_pair: scaled_track_bandwidths) {
    double next_target_allocation = scaled_track_bandwidth_pair.first;
    double allocation_range = next_target_allocation - last_target_allocation;
    // How much bitrate is allocated to all tracks within the current scaled
    // target allocation range.
    double current_range_allocation = track_allocation_factor * allocation_range;

    // We have reached a point where we can calculate the target_allocation.
    if (current_range_allocation > remaining_bitrate)
      break;

    // Update the current point we are at and the remaining bitrate.
    last_target_allocation = next_target_allocation;
    remaining_bitrate -= current_range_allocation;
    track_allocation_factor -= scaled_track_bandwidth_pair.second;
  }
  double target_allocation = last_target_allocation + (remaining_bitrate / track_allocation_factor);
  return DistributeBitrateByTargetAllocation(target_allocation, track_configs);
}

std::vector<uint32_t> PriorityBasedAllocationStrategy::MaxRateAllocation(
    uint32_t available_bitrate,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*> track_configs) {

  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    track_allocations.push_back(track_config->max_bitrate_bps);
  }

  return track_allocations;
}

std::vector<uint32_t> PriorityBasedAllocationStrategy::DistributeBitrateByTargetAllocation(
    double target_allocation,
    const ArrayView<const BitrateAllocationStrategy::TrackConfig*> track_configs) {
  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    double scaled_allocation = track_priority_map_[track_config->track_id] * target_allocation;
    double track_allocation = std::min(
        static_cast<double>(track_config->max_bitrate_bps),
        scaled_allocation + static_cast<double>(track_config->min_bitrate_bps));
    track_allocations.push_back(track_allocation);
  }
  return track_allocations;
}

}  // namespace rtc
