/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_H_
#define API_TEST_NETEQ_SIMULATOR_H_

namespace webrtc {
namespace test {

class NetEqSimulator {
 public:
  virtual ~NetEqSimulator() = default;

  // The results of one simulation step.
  struct SimulationStepResult {
    enum class SimulationPosition { kFinished = 0, kRunning = 1 };
    SimulationPosition simulation_position = SimulationPosition::kRunning;
    int normal_ms = 0;
    int expand_ms = 0;
    int accelerate_ms = 0;
    int preemptive_expand_ms = 0;
    int64_t simulation_step_ms = 0;
  };

  enum class NextActionResult { kIllegal = 0, kUnwise = 1, kNormal = 2 };

  enum class Action { kNormal = 0, kExpand, kAccelerate, kPreemptiveExpand };

  struct SimulationState {
    // The sum of the packet buffer and sync buffer delay.
    int current_delay_ms_ = 0;
    // Mean inter-arrival time between packets.
    int mean_iat_ms_ = 0;
    // TODO(ivoc): Expand this struct with more useful metrics.
  };

  // Runs the simulation until we hit the next GetAudio event. Will return
  // kRunning if a GetAudio event was found, or kFinished when the simulation is
  // done and no more GetAudio event could be found.
  virtual SimulationStepResult RunToNextGetAudio() = 0;

  // Set the next action to be taken by NetEq. This will overwrite any action
  // that NetEq would normally decide to take.
  virtual NextActionResult SetNextAction(Action next_operation) = 0;

  // Obtain statistics.
  virtual SimulationState GetSimulationState() = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETEQ_SIMULATOR_H_
