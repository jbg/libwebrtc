/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_API_NETWORK_PREDICTOR_H_
#define THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_API_NETWORK_PREDICTOR_H_

#include <memory>
#include <vector>

namespace webrtc {
// TODO(yinwa): work in progress. API in class FecController should not be
// used by other users until this comment is removed.

// NetworkPredictor predict network state based on current network metrics.
// Usage:
// Setup by calling Initialize.
// For each update, call RunInference. RunInference returns network state
// prediction.
class NetworkPredictor {
 public:
  virtual ~NetworkPredictor() {}

  // Initialize network state predictor.
  virtual void Initialize() = 0;

  // Returns current network state prediction.
  // Inputs:  delay_time_sec - the one way network delay in seconds.
  //          delay_time_delta_sec - the delta of current one way delay and last
  //          one way delay in sec. rtts_sec - round trip time in seconds.
  //          network_state - computed network state.
  virtual int64_t RunInference(float delay_time_sec,
                               float delay_time_delta_sec,
                               float rtts_sec,
                               int network_state) = 0;
};

class NetworkPredictorFactoryInterface {
 public:
  virtual std::unique_ptr<NetworkPredictor> CreateNetworkPredictor() = 0;
  virtual ~NetworkPredictorFactoryInterface() = default;
};

}  // namespace webrtc

#endif  // THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_API_NETWORK_PREDICTOR_H_
