/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_LOGGING_MEMORY_LOG_WRITER_H_
#define TEST_LOGGING_MEMORY_LOG_WRITER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/memory_stream.h"
#include "test/logging/log_writer.h"

namespace webrtc {

class MemoryLogWriterManager {
 public:
  MemoryLogWriterManager();
  std::unique_ptr<LogWriterFactoryInterface> CreateFactory();
  const std::map<std::string, std::string>& logs() { return finalized_; }

 private:
  std::map<std::string, std::string> finalized_;
};

}  // namespace webrtc

#endif  // TEST_LOGGING_MEMORY_LOG_WRITER_H_
