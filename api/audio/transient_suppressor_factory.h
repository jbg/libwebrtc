/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_TRANSIENT_SUPPRESSOR_FACTORY_H_
#define API_AUDIO_TRANSIENT_SUPPRESSOR_FACTORY_H_

#include <memory>

#include "api/audio/transient_suppressor.h"

namespace webrtc {

std::unique_ptr<TransientSuppressor> CreateTransientSuppressor();

}  // namespace webrtc

#endif  // API_AUDIO_TRANSIENT_SUPPRESSOR_FACTORY_H_
