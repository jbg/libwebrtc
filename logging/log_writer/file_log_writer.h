/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_LOG_WRITER_FILE_LOG_WRITER_H_
#define LOGGING_LOG_WRITER_FILE_LOG_WRITER_H_

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

#include "api/log_writer_impl.h"

namespace webrtc {
namespace webrtc_impl {
class FileLogWriter final : public LogWriterImplInterface {
 public:
  explicit FileLogWriter(std::string file_path);
  WriteResult Write(absl::string_view value) override;
  ~FileLogWriter() final;
  void Flush() override;

 private:
  std::FILE* const out_;
};
}  // namespace webrtc_impl
class FileLogWriterManager final : public LogWriterImplManagerInterface {
 public:
  explicit FileLogWriterManager(std::string base_path);
  ~FileLogWriterManager() final;

  LogWriterImplInterface* Create(std::string filename) override;
  void Destroy(LogWriterImplInterface* writer) override;

 private:
  const std::string base_path_;
  rtc::CriticalSection crit_sect_;
  std::vector<std::unique_ptr<webrtc_impl::FileLogWriter>> writers_
      RTC_GUARDED_BY(crit_sect_);
};

}  // namespace webrtc

#endif  // LOGGING_LOG_WRITER_FILE_LOG_WRITER_H_
