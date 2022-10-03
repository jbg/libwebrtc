/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SPEECH_PROBABILITY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SPEECH_PROBABILITY_BUFFER_H_

#include <vector>

namespace webrtc {

// This class implements a circular buffer that stores speech probabilities
// for a speech segment and estimates speech activity for that segment.
class SpeechProbabilityBuffer {
 public:
  // Ctor. Sets the buffer capacity to max(0, `capacity`). The value of
  // `low_probability_threshold` required to be on the range [0.0f, 1.0f].
  explicit SpeechProbabilityBuffer(int capacity,
                                   float low_probability_threshold);
  ~SpeechProbabilityBuffer() {}
  SpeechProbabilityBuffer(const SpeechProbabilityBuffer&) = delete;
  SpeechProbabilityBuffer& operator=(const SpeechProbabilityBuffer&) = delete;

  // Inserts speech probability and update the sum of probabilities. The value
  // of `probability` expected to be on the range [0.0f, 1.0f].
  void Update(float probability);

  // Resets the histogram, forget the past.
  void Reset();

  // Returns true if the segment is active (a long enough segment with an
  // average speech probability above `low_probability_threshold` after
  // transient removal).
  bool IsActiveSegment() const;

  // Use only for testing.
  float GetSumProbabilities() const { return sum_probabilities_; }
  int GetBufferCapacity() const { return probabilities_.size(); }

 private:
  void RemoveOldestEntry();
  void AddNewEntry(float probability);
  void RemoveTransient();

  const float low_probability_threshold_;

  // Sum of probabilities stored in `probabilities_`. Must be updated if
  // `probabilities_` is updated.
  float sum_probabilities_ = 0.0f;

  // Circular buffer for probabilities.
  std::vector<float> probabilities_;

  // Current index of circular buffer, where the newest data will be written to,
  // therefore, pointing to the oldest data if buffer is full.
  int buffer_index_ = 0;

  // Indicates if buffer is full and we had a wrap around.
  int buffer_is_full_ = false;

  int len_high_activity_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SPEECH_PROBABILITY_BUFFER_H_
