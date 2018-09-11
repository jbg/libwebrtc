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

// TODO: !!! Rename, move, document...
template <typename T>
void EncodeDeltas(T base, std::vector<T> values, std::string* output) {}

// TODO: !!! Rename, move, document...
template <typename T>
std::vector<T> Decode(const std::string input, T base, size_t num_of_deltas) {
  return std::vector<T>();
}

// TODO: !!! Move?
class DeltaEncoder {
 public:
  virtual ~DeltaEncoder() = default;

  // Encode |values| as a sequence of delta following on |base|, and writes it
  // into |output|. Return value indicates whether the operation was successful.
  // |base| is not guaranteed to be written into |output|, and must therefore
  // be provided separately to the decoder.
  // Encode() must write into |output| a bit pattern that would allow the
  // decoder to distinguish what type of DeltaEncoder produced it.
  virtual bool Encode(uint64_t base,
                      std::vector<uint64_t> values,
                      std::string* output) = 0;
};

// class DeltaDecoder final {
//  public:

//  private:
// };

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_DELTA_ENCODER_H_
