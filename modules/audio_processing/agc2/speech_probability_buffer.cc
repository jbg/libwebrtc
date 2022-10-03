/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_probability_buffer.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

constexpr float kActivityThreshold = 0.3f;
constexpr int kTransientWidthThreshold = 7;

SpeechProbabilityBuffer::SpeechProbabilityBuffer(
    int capacity,
    float low_probability_threshold)
    : low_probability_threshold_(low_probability_threshold),
      probabilities_(std::max(capacity, 0)) {
  RTC_DCHECK_GE(low_probability_threshold, 0.0f);
  RTC_DCHECK_LE(low_probability_threshold, 1.0f);
}

void SpeechProbabilityBuffer::Update(float probability) {
  // Remove the oldest entry if the circular buffer is not empty.
  if (probabilities_.size() > 0) {
    RemoveOldestEntry();
  }

  AddNewEntry(probability);
}

void SpeechProbabilityBuffer::RemoveOldestEntry() {
  RTC_DCHECK_GT(probabilities_.size(), 0);
  // Do nothing if circular buffer is not full.
  if (!buffer_is_full_) {
    return;
  }

  float oldest_probability = probabilities_[buffer_index_];
  sum_probabilities_ -= oldest_probability;
}

void SpeechProbabilityBuffer::RemoveTransient() {
  // Don't expect to be here if high-activity region is longer than
  // `kTransientWidthThreshold` or there has not been any transient.
  RTC_DCHECK_LE(len_high_activity_, kTransientWidthThreshold);

  // Reset short circular buffers.
  if (probabilities_.size() <= kTransientWidthThreshold) {
    Reset();
    return;
  }

  // Remove previously added probabilities.
  int index = buffer_index_ > 0 ? buffer_index_ - 1 : probabilities_.size() - 1;

  while (len_high_activity_ > 0) {
    sum_probabilities_ -= probabilities_[index];
    probabilities_[index] = 0.0f;

    // Update the circular  buffer index.
    index = index > 0 ? index - 1 : probabilities_.size() - 1;
    --len_high_activity_;
  }
}

void SpeechProbabilityBuffer::AddNewEntry(float probability) {
  // Zero out low probability.
  if (probability < low_probability_threshold_) {
    probability = 0.0f;
  }

  if (probabilities_.size() == 0) {
    sum_probabilities_ = probability;
    return;
  }

  // Check for transients and zero out low probabilities.
  if (probability <= low_probability_threshold_) {
    // Lower than threshold probability, set it to zero.
    probability = 0.0f;

    // Check if this has been a transient.
    if (len_high_activity_ <= kTransientWidthThreshold) {
      RemoveTransient();
    }
    len_high_activity_ = 0;
  } else if (len_high_activity_ <= kTransientWidthThreshold) {
    ++len_high_activity_;
  }

  // Update the circular buffer and the current sum.
  probabilities_[buffer_index_] = probability;
  sum_probabilities_ += probability;

  // Increment the buffer index and check for wrap-around.
  if (++buffer_index_ >= static_cast<int>(probabilities_.size())) {
    buffer_index_ = 0;
    buffer_is_full_ = true;
  }
}

bool SpeechProbabilityBuffer::IsActiveSegment() const {
  if (!buffer_is_full_) {
    return false;
  }
  if (sum_probabilities_ < kActivityThreshold * probabilities_.size()) {
    return false;
  }
  return true;
}

void SpeechProbabilityBuffer::Reset() {
  sum_probabilities_ = 0.0f;

  // Empty the circular buffer.
  buffer_index_ = 0;
  buffer_is_full_ = false;
  len_high_activity_ = 0;
}

}  // namespace webrtc
