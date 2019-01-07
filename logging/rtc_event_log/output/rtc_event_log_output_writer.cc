/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/rtc_event_log/output/rtc_event_log_output_writer.h"

#include <utility>

namespace webrtc {

RtcEventLogOutputLogWriter::RtcEventLogOutputLogWriter(
    std::unique_ptr<LogWriter> writer)
    : writer_(std::move(writer)) {}

RtcEventLogOutputLogWriter::~RtcEventLogOutputLogWriter() {}

bool RtcEventLogOutputLogWriter::IsActive() const {
  return writer_ != nullptr;
}

bool RtcEventLogOutputLogWriter::Write(const std::string& output) {
  return writer_->TryWrite(output);
}

}  // namespace webrtc
