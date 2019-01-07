/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/log_writer/memory_log_writer.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_impl {

MemoryLogWriter::MemoryLogWriter(MemoryLogWriterManager* manager,
                                 std::string filename)
    : manager_(manager), filename_(filename) {
  chunks_.emplace_back();
}

MemoryLogWriter::~MemoryLogWriter() {
  manager_->Deliver(filename_, chunks_);
}

LogWriterImplInterface::WriteResult MemoryLogWriter::Write(
    absl::string_view value) {
  const size_t kSoftChunkSizeLimit = 1000 * 1000;
  if (value.size() >= kSoftChunkSizeLimit - chunks_.back().size()) {
    chunks_.back().shrink_to_fit();
    chunks_.emplace_back(value);
  } else {
    if (chunks_.back().size() > kSoftChunkSizeLimit) {
      chunks_.back().shrink_to_fit();
      chunks_.emplace_back();
      chunks_.back().reserve(kSoftChunkSizeLimit);
    }
    chunks_.back().append(value.data(), value.size());
  }
  return LogWriterImplInterface::WriteResult::kSuccess;
}

void MemoryLogWriter::Flush() {}

MemoryLogWriterFactory::MemoryLogWriterFactory(MemoryLogWriterManager* manager)
    : manager_(manager) {}

std::unique_ptr<LogWriterImplInterface> MemoryLogWriterFactory::Create(
    std::string filename) {
  return absl::make_unique<MemoryLogWriter>(manager_, filename);
}

MemoryLogWriterFactory::~MemoryLogWriterFactory() {}

}  // namespace webrtc_impl

MemoryLogWriterManager::MemoryLogWriterManager() {}

std::unique_ptr<LogWriterImplFactoryInterface>
MemoryLogWriterManager::CreateFactory() {
  return absl::make_unique<webrtc_impl::MemoryLogWriterFactory>(this);
}

void MemoryLogWriterManager::Deliver(std::string name,
                                     const std::vector<std::string>& chunks) {
  size_t total_size = 0;
  for (auto& chunk : chunks)
    total_size += chunk.size();
  finalized_[name].reserve(total_size);

  for (auto& chunk : chunks)
    finalized_[name].append(chunk);
}

// namespace webrtc_impl
}  // namespace webrtc
