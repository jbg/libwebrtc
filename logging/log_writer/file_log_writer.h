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

#include "api/log_writer_impl.h"

namespace webrtc {
namespace webrtc_impl {
class FileLogWriter final : public LogWriterImplInterface {
 public:
  explicit FileLogWriter(std::string file_path);
  ~FileLogWriter() final;
  WriteResult Write(absl::string_view value) override;
  void Flush() override;

 private:
  std::FILE* const out_;
};
}  // namespace webrtc_impl
class FileLogWriterManager final : public LogWriterImplFactoryInterface {
 public:
  explicit FileLogWriterManager(std::string base_path);
  ~FileLogWriterManager() final;

  std::unique_ptr<LogWriterImplInterface> Create(std::string filename) override;

 private:
  const std::string base_path_;
  std::vector<std::unique_ptr<webrtc_impl::FileLogWriter>> writers_;
};

}  // namespace webrtc

#endif  // LOGGING_LOG_WRITER_FILE_LOG_WRITER_H_
