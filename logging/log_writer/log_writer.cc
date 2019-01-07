/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/log_writer/log_writer.h"
#include "absl/memory/memory.h"

namespace webrtc {

LogWriterFactory::LogWriterFactory(
    std::unique_ptr<LogWriterImplManagerInterface> impl)
    : impl_(std::move(impl)) {}

LogWriterFactory::~LogWriterFactory() {}

std::unique_ptr<LogWriter> LogWriterFactory::Create(std::string filename) {
  return absl::make_unique<LogWriter>(impl_.get(), filename);
}

}  // namespace webrtc
