/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_LOGGING_LOG_WRITER_H_
#define TEST_LOGGING_LOG_WRITER_H_

#include <memory>
#include <string>
#include <utility>

#include "api/rtceventlogoutput.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
template <class... Args>
inline void LogWriteFormat(RtcEventLogOutput* out_,
                           const char* fmt,
                           Args&&... args) {
  rtc::StringBuilder sb;
  sb.AppendFormat(fmt, std::forward<Args>(args)...);
  out_->Write(sb.str());
}

class LogWriterFactoryInterface {
 public:
  virtual std::unique_ptr<RtcEventLogOutput> Create(std::string filename) = 0;
  virtual ~LogWriterFactoryInterface() = default;
};

class LogWriterFactoryAddName : public LogWriterFactoryInterface {
 public:
  LogWriterFactoryAddName(LogWriterFactoryInterface* base,
                          std::string add_name);
  std::unique_ptr<RtcEventLogOutput> Create(std::string filename) override;

 private:
  LogWriterFactoryInterface* const base_factory_;
  const std::string add_name_;
};

}  // namespace webrtc

#endif  // TEST_LOGGING_LOG_WRITER_H_
