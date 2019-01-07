/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_WRITER_H_
#define LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_WRITER_H_

#include <memory>
#include <string>

#include "api/rtceventlogoutput.h"
#include "logging/log_writer/log_writer.h"

namespace webrtc {

class RtcEventLogOutputLogWriter final : public RtcEventLogOutput {
 public:
  explicit RtcEventLogOutputLogWriter(std::unique_ptr<LogWriter> writer);
  ~RtcEventLogOutputLogWriter() override;
  bool IsActive() const override;
  bool Write(const std::string& output) override;

 private:
  std::unique_ptr<LogWriter> writer_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_WRITER_H_
