/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/log_writer/file_log_writer.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace webrtc_impl {

FileLogWriter::FileLogWriter(std::string file_path)
    : out_(std::fopen(file_path.c_str(), "w")) {
  RTC_CHECK(out_ != nullptr)
      << "Failed to open file: '" << file_path << "' for writing.";
}

FileLogWriter::~FileLogWriter() {
  std::fclose(out_);
}

LogWriterImplInterface::WriteResult FileLogWriter::Write(
    absl::string_view value) {
  size_t written = std::fwrite(value.data(), 1, value.size(), out_);
  if (written == value.size()) {
    return LogWriterImplInterface::WriteResult::kSuccess;
  } else {
    return LogWriterImplInterface::WriteResult::kUnknownError;
  }
}

void FileLogWriter::Flush() {
  fflush(out_);
}

}  // namespace webrtc_impl

FileLogWriterManager::FileLogWriterManager(std::string base_path)
    : base_path_(base_path) {
  for (size_t i = 0; i < base_path.size(); ++i) {
    if (base_path[i] == '/')
      test::CreateDir(base_path.substr(0, i));
  }
}

FileLogWriterManager::~FileLogWriterManager() {}

LogWriterImplInterface* FileLogWriterManager::Create(std::string filename) {
  auto* writer = new webrtc_impl::FileLogWriter(base_path_ + filename);
  rtc::CritScope cs(&crit_sect_);
  writers_.emplace_back(writer);
  return writer;
}

void FileLogWriterManager::Destroy(LogWriterImplInterface* writer) {
  rtc::CritScope cs(&crit_sect_);
  for (auto it = writers_.begin(); it != writers_.end(); ++it) {
    if (it->get() == writer) {
      writers_.erase(it);
      break;
    }
  }
}
}  // namespace webrtc
