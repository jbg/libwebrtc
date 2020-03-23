/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/transient/transient_suppressor_factory.h"

#include <memory>

#ifndef WEBRTC_EXCLUDE_TRANSIENT_SUPPRESSOR
#include "modules/audio_processing/transient/transient_suppressor_impl.h"
#endif

namespace webrtc {

std::unique_ptr<TransientSuppressor> CreateTransientSuppressor() {
#ifndef WEBRTC_EXCLUDE_TRANSIENT_SUPPRESSOR
  return std::make_unique<TransientSuppressorImpl>();
#else
  return nullptr;
#endif
}

}  // namespace webrtc
