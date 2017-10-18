/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_
#define RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_

#include "rtc_base/bitrateallocationstrategy.h"
#include "rtc_base/checks.h"

namespace rtc {

// Pluggable strategy allows configuration of bitrate allocation per media
// track.
// TODO (shampson): Create a better definition of the priority allocation
//                  strategy
class PriorityBasedAllocationStrategy
    : public BitrateAllocationStrategy {
 public:
  PriorityBasedAllocationStrategy(
          std::map<std::string, double> track_priority_map);

  std::vector<uint32_t> AllocateBitrates(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs) override;
  inline ~PriorityBasedAllocationStrategy() override {}

 private:
  // Maps each track from its track_id to the track's relative_bitrate. The
  // relative_bitrate is the relative priority of bandwidth allocation
  std::map<std::string, double> track_priority_map_;

  // Allocate bitrate to tracks when sum of track's min_bitrate_bps is not
  // satisfied.
  std::vector<uint32_t> LowRateAllocationByPriority(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Allocate bitrate to tracks when the available bitrate is between the sum
  // of the min and max bitrates of each track. The strategy here is to
  // allocate the bitrate based upon the relative_bitrate of each track. This
  // relative bitrate defines the priority for bitrate to be allocated to that
  // track in relation to other tracks. For example with two tracks, if track
  // 1 had a relative_bitrate = 1.0, and track 2 has a relative_bitrate of 2.0,
  // the expected behavior is that track 2 will be allocated double the bitrate
  // as track 1 above their min_bitrate_bps values, until one of the tracks hit
  // its max_bitrate_bps.
  std::vector<uint32_t> NormalRateAllocationByPriority(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Allocate the max bitrate to each track when there is sufficient available
  // bitrate.
  std::vector<uint32_t> MaxRateAllocation(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs);
  // Calculate and allocate each track's bitrate based upon the target_allocation.
  // Each track is allocated min(max_bps, target_allocation * relative_bitrate).
  std::vector<uint32_t> DistributeBitrateByTargetAllocation(
      double target_allocation,
      const ArrayView<const TrackConfig*> track_configs);
};

}  // namespace rtc

#endif  // RTC_BASE_PRIORITYBASEDALLOCATIONSTRATEGY_H_