/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_LOG_WRITER_LOG_WRITER_H_
#define LOGGING_LOG_WRITER_LOG_WRITER_H_

#include <memory>
#include <string>
#include <utility>

#include "api/log_writer_impl.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
class LogWriter {
 public:
  LogWriter(LogWriterImplFactoryInterface* manager, std::string filename)
      : manager_(manager), impl_(manager_->Create(filename)) {}
  LogWriter(const LogWriter&) = delete;
  LogWriter& operator=(const LogWriter&) = delete;
  ~LogWriter() {}
  bool TryWrite(absl::string_view value) {
    auto res = impl_->Write(value);
    return res == LogWriterImplInterface::WriteResult::kSuccess;
  }
  void Write(absl::string_view value) { impl_->Write(value); }
  void Flush() { impl_->Flush(); }

  template <class... Args>
  void Format(const char* fmt, Args&&... args) {
    rtc::StringBuilder sb;
    sb.AppendFormat(fmt, std::forward<Args>(args)...);
    // TODO(srte): Add formatted output override to LogWriterImplInterface.
    impl_->Write(sb.str());
  }

 private:
  LogWriterImplFactoryInterface* const manager_;
  std::unique_ptr<LogWriterImplInterface> const impl_;
};

class LogWriterFactory {
 public:
  explicit LogWriterFactory(
      std::unique_ptr<LogWriterImplFactoryInterface> impl);
  ~LogWriterFactory();
  std::unique_ptr<LogWriter> Create(std::string filename);

 private:
  const std::unique_ptr<LogWriterImplFactoryInterface> impl_;
};
}  // namespace webrtc

#endif  // LOGGING_LOG_WRITER_LOG_WRITER_H_
