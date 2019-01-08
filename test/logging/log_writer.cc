/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/logging/log_writer.h"

namespace webrtc {

LogWriterFactoryAddName::LogWriterFactoryAddName(
    LogWriterFactoryInterface* base,
    std::string add_name)
    : base_factory_(base), add_name_(add_name) {}

std::unique_ptr<RtcEventLogOutput> LogWriterFactoryAddName::Create(
    std::string filename) {
  return base_factory_->Create(add_name_ + filename);
}

}  // namespace webrtc
