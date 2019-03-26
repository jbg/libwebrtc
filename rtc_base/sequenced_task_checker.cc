/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/checks.h"

namespace rtc {
namespace internal {

SequencedTaskCheckerScope::SequencedTaskCheckerScope(
    const SequencedTaskChecker* checker) {
  RTC_DCHECK(checker->CalledSequentially());
}

SequencedTaskCheckerScope::~SequencedTaskCheckerScope() {}

}  // namespace internal
}  // namespace rtc
