/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_
#define MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/rtt_filter.h"
#include "rtc_base/rolling_accumulator.h"

namespace webrtc {

class Clock;

class VCMJitterEstimator {
 public:
  explicit VCMJitterEstimator(Clock* clock);
  virtual ~VCMJitterEstimator();
  VCMJitterEstimator& operator=(const VCMJitterEstimator& rhs);

  // Resets the estimate to the initial state.
  void Reset();

  // Updates the jitter estimate with the new data.
  //
  // Input:
  //          - frameDelay      : Delay-delta calculated by UTILDelayEstimate.
  //          - frameSize       : Frame size of the current frame.
  //          - incompleteFrame : Flags if the frame is used to update the
  //                              estimate before it was complete.
  //                              Default is false.
  void UpdateEstimate(TimeDelta frameDelay,
                      uint32_t frameSizeBytes,
                      bool incompleteFrame = false);

  // Returns the current jitter estimate and adds an RTT dependent term in cases
  // of retransmission.
  //  Input:
  //          - rttMultiplier  : RTT param multiplier (when applicable).
  //          - rttMultAddCap  : TODO
  //
  // Return value              : Jitter estimate.
  virtual TimeDelta GetJitterEstimate(double rttMultiplier,
                                      absl::optional<TimeDelta> rttMultAddCap);

  // Updates the nack counter.
  void FrameNacked();

  // Updates the RTT filter.
  //
  // Input:
  //          - rtt          : Round trip time.
  void UpdateRtt(TimeDelta rtt);

  // A constant describing the delay from the jitter buffer to the delay on the
  // receiving side which is not accounted for by the jitter buffer nor the
  // decoding delay estimate.
  static constexpr TimeDelta OPERATING_SYSTEM_JITTER = TimeDelta::Millis(10);

 protected:
  // These are protected for better testing possibilities.
  double _theta[2];  // Estimated line parameters (slope, offset)
  double _varNoise;  // Variance of the time-deviation from the line

 private:
  // Updates the Kalman filter for the line describing the frame size dependent
  // jitter.
  //
  // Input:
  //          - frameDelay      : Delay-delta calculated by UTILDelayEstimate.
  //          - deltaFSBytes    : Frame size delta, i.e. frame size at time T
  //                            : minus frame size at time T-1.
  void KalmanEstimateChannel(TimeDelta frameDelay, int32_t deltaFSBytes);

  // Updates the random jitter estimate, i.e. the variance of the time
  // deviations from the line given by the Kalman filter.
  //
  // Input:
  //          - d_dT              : The deviation from the kalman estimate.
  //          - incompleteFrame   : True if the frame used to update the
  //                                estimate with was incomplete.
  void EstimateRandomJitter(double d_dT, bool incompleteFrame);

  double NoiseThreshold() const;

  // Calculates the current jitter estimate.
  //
  // Return value                 : The current jitter estimate.
  TimeDelta CalculateEstimate();

  // Post process the calculated estimate.
  void PostProcessEstimate();

  // Calculates the difference in delay between a sample and the expected delay
  // estimated by the Kalman filter.
  //
  // Input:
  //          - frameDelay      : Delay-delta calculated by UTILDelayEstimate.
  //          - deltaFS         : Frame size delta, i.e. frame size at time
  //                              T minus frame size at time T-1.
  //
  // Return value               : The difference in milliseconds.
  double DeviationFromExpectedDelay(TimeDelta frameDelay,
                                    int32_t deltaFSBytes) const;

  Frequency GetFrameRate() const;

  // Constants, filter parameters.
  const double _phi;
  const double _psi;
  const uint32_t _alphaCountMax;
  const double _thetaLow;
  const uint32_t _nackLimit;
  const int32_t _numStdDevDelayOutlier;
  const int32_t _numStdDevFrameSizeOutlier;
  const double _noiseStdDevs;
  const double _noiseStdDevOffset;

  double _thetaCov[2][2];  // Estimate covariance
  double _Qcov[2][2];      // Process noise covariance
  double _avgFrameSize;    // Average frame size
  double _varFrameSize;    // Frame size variance
  double _maxFrameSize;    // Largest frame size received (descending
                           // with a factor _psi)
  uint32_t _fsSum;
  uint32_t _fsCount;

  absl::optional<Timestamp> _lastUpdateT;
  // The previously returned jitter estimate
  absl::optional<TimeDelta> _prevEstimate;
  uint32_t _prevFrameSize;  // Frame size of the previous frame
  double _avgNoise;         // Average of the random jitter
  uint32_t _alphaCount;
  // The filtered sum of jitter estimates
  TimeDelta _filterJitterEstimate = TimeDelta::Zero();

  uint32_t _startupCount;
  // Time when the latest nack was seen
  Timestamp _latestNack = Timestamp::Zero();
  // Keeps track of the number of nacks received,
  // but never goes above _nackLimit
  uint32_t _nackCount;
  VCMRttFilter _rttFilter;

  // Tracks frame rates in microseconds.
  rtc::RollingAccumulator<uint64_t> fps_counter_;
  const double time_deviation_upper_bound_;
  const bool enable_reduced_delay_;
  Clock* clock_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_
