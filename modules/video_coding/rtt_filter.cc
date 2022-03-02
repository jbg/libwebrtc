/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtt_filter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"
#include "api/units/time_delta.h"

namespace webrtc {

namespace {

constexpr TimeDelta kMaxRtt = TimeDelta::Seconds(3);

}

VCMRttFilter::VCMRttFilter()
    : _avgRtt(TimeDelta::Zero()),
      _varRtt(0),
      _maxRtt(TimeDelta::Zero()),
      _filtFactMax(35),
      _jumpStdDevs(2.5),
      _driftStdDevs(3.5),
      _detectThreshold(kMaxDriftJumpCount),
      _jumpBuf(kMaxDriftJumpCount, TimeDelta::Zero()),
      _driftBuf(kMaxDriftJumpCount, TimeDelta::Zero()) {
  Reset();
}

VCMRttFilter& VCMRttFilter::operator=(const VCMRttFilter& rhs) {
  if (this != &rhs) {
    _gotNonZeroUpdate = rhs._gotNonZeroUpdate;
    _avgRtt = rhs._avgRtt;
    _varRtt = rhs._varRtt;
    _maxRtt = rhs._maxRtt;
    _filtFactCount = rhs._filtFactCount;
    _jumpBuf = rhs._jumpBuf;
    _driftBuf = rhs._driftBuf;
  }
  return *this;
}

void VCMRttFilter::Reset() {
  _gotNonZeroUpdate = false;
  _avgRtt = TimeDelta::Zero();
  _varRtt = 0;
  _maxRtt = TimeDelta::Zero();
  _filtFactCount = 1;
  absl::c_fill(_jumpBuf, TimeDelta::Zero());
  absl::c_fill(_driftBuf, TimeDelta::Zero());
}

void VCMRttFilter::Update(TimeDelta rtt) {
  if (!_gotNonZeroUpdate) {
    if (rtt.IsZero()) {
      return;
    }
    _gotNonZeroUpdate = true;
  }

  // Sanity check
  if (rtt > kMaxRtt) {
    rtt = kMaxRtt;
  }

  double filtFactor = 0;
  if (_filtFactCount > 1) {
    filtFactor = static_cast<double>(_filtFactCount - 1) / _filtFactCount;
  }
  _filtFactCount++;
  if (_filtFactCount > _filtFactMax) {
    // This prevents filtFactor from going above
    // (_filtFactMax - 1) / _filtFactMax,
    // e.g., _filtFactMax = 50 => filtFactor = 49/50 = 0.98
    _filtFactCount = _filtFactMax;
  }
  TimeDelta oldAvg = _avgRtt;
  int64_t oldVar = _varRtt;
  _avgRtt = filtFactor * _avgRtt + (1 - filtFactor) * rtt;
  int64_t deltaMs = (rtt - _avgRtt).ms();
  _varRtt = filtFactor * _varRtt + (1 - filtFactor) * (deltaMs * deltaMs);
  _maxRtt = std::max(rtt, _maxRtt);
  if (!JumpDetection(rtt) || !DriftDetection(rtt)) {
    // In some cases we don't want to update the statistics
    _avgRtt = oldAvg;
    _varRtt = oldVar;
  }
}

bool VCMRttFilter::JumpDetection(TimeDelta rtt) {
  TimeDelta diffFromAvg = _avgRtt - rtt;
  // Unit of sqrt of _varRtt is ms.
  TimeDelta jumpThreshold = TimeDelta::Millis(_jumpStdDevs * sqrt(_varRtt));
  if (diffFromAvg.Abs() > jumpThreshold) {
    int diffSign = (diffFromAvg >= TimeDelta::Zero()) ? 1 : -1;
    int jumpCountSign = (_jumpBuf.size() >= 0) ? 1 : -1;
    if (diffSign != jumpCountSign) {
      // Since the signs differ the samples currently
      // in the buffer is useless as they represent a
      // jump in a different direction.
      _jumpBuf.clear();
    }
    if (_jumpBuf.size() < kMaxDriftJumpCount) {
      // Update the buffer used for the short time statistics.
      // The sign of the diff is used for updating the counter since
      // we want to use the same buffer for keeping track of when
      // the RTT jumps down and up.
      _jumpBuf.push_back(rtt);
    }
    if (_jumpBuf.size() >= _detectThreshold) {
      // Detected an RTT jump
      ShortRttFilter(_jumpBuf);
      _filtFactCount = _detectThreshold + 1;
      _jumpBuf.clear();
    } else {
      return false;
    }
  } else {
    _jumpBuf.clear();
  }
  return true;
}

bool VCMRttFilter::DriftDetection(TimeDelta rtt) {
  // Unit of sqrt of _varRtt is ms.
  TimeDelta driftThreshold = TimeDelta::Millis(_driftStdDevs * sqrt(_varRtt));
  if (_maxRtt - _avgRtt > driftThreshold) {
    if (_driftBuf.size() < kMaxDriftJumpCount) {
      // Update the buffer used for the short time
      // statistics.
      _driftBuf.push_back(rtt);
    }
    if (_driftBuf.size() >= _detectThreshold) {
      // Detected an RTT drift
      ShortRttFilter(_driftBuf);
      _filtFactCount = _detectThreshold + 1;
      _driftBuf.clear();
    }
  } else {
    _driftBuf.clear();
  }
  return true;
}

void VCMRttFilter::ShortRttFilter(
    const absl::InlinedVector<TimeDelta, kMaxDriftJumpCount>& buf) {
  if (buf.empty()) {
    return;
  }
  _maxRtt = TimeDelta::Zero();
  _avgRtt = TimeDelta::Zero();
  for (const TimeDelta& rtt : buf) {
    if (rtt > _maxRtt) {
      _maxRtt = rtt;
    }
    _avgRtt += rtt;
  }
  _avgRtt = _avgRtt / static_cast<double>(buf.size());
}

TimeDelta VCMRttFilter::Rtt() const {
  return _maxRtt;
}
}  // namespace webrtc
