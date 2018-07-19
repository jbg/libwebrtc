/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsepicecandidate.h"

namespace webrtc {

std::string JsepIceCandidate::sdp_mid() const {
  return sdp_mid_;
}

int JsepIceCandidate::sdp_mline_index() const {
  return sdp_mline_index_;
}

const cricket::Candidate& JsepIceCandidate::candidate() const {
  return candidate_;
}

std::string JsepIceCandidate::server_url() const {
  return candidate_.url();
}

JsepCandidateCollection::JsepCandidateCollection() = default;

JsepCandidateCollection::JsepCandidateCollection(JsepCandidateCollection&& o)
    : candidates_(std::move(o.candidates_)) {}

size_t JsepCandidateCollection::count() const {
  return candidates_.size();
}

void JsepCandidateCollection::add(JsepIceCandidate* candidate) {
  candidates_.push_back(candidate);
}

const IceCandidateInterface* JsepCandidateCollection::at(size_t index) const {
  return candidates_[index];
}

}  // namespace webrtc
