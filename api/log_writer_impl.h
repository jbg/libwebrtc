/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_LOG_WRITER_IMPL_H_
#define API_LOG_WRITER_IMPL_H_
#include <string>
#include "absl/strings/string_view.h"

namespace webrtc {
class LogWriterImplInterface {
 public:
  enum class WriteResult { kSuccess, kUnknownError, kStorageFull };
  virtual WriteResult Write(absl::string_view value) = 0;
  virtual void Flush() = 0;
  virtual ~LogWriterImplInterface() = default;
};

class LogWriterImplManagerInterface {
 public:
  virtual LogWriterImplInterface* Create(std::string filename) = 0;
  virtual void Destroy(LogWriterImplInterface* writer) = 0;
  virtual ~LogWriterImplManagerInterface() = default;
};
}  // namespace webrtc

#endif  // API_LOG_WRITER_IMPL_H_
