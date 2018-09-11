/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_DELTA_ENCODER_H_
#define LOGGING_RTC_EVENT_LOG_DELTA_ENCODER_H_

#include <string>
#include <vector>

namespace webrtc {

// TODO: !!! Document
void EncodeDeltas(uint64_t base,
                  std::vector<uint64_t> values,
                  std::string* output);

// TODO: !!! Document
std::vector<uint64_t> Decode(const std::string input,
                             uint64_t base,
                             size_t num_of_deltas);

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_DELTA_ENCODER_H_
