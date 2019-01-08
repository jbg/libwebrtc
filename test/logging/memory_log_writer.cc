/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/logging/memory_log_writer.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
namespace webrtc {
namespace webrtc_impl {

MemoryLogWriter::MemoryLogWriter(MemoryLogWriterManager* manager,
                                 std::string filename)
    : manager_(manager), filename_(filename) {}

MemoryLogWriter::~MemoryLogWriter() {
  manager_->Deliver(filename_, buffer_);
}

bool MemoryLogWriter::IsActive() const {
  return true;
}

bool MemoryLogWriter::Write(const std::string& value) {
  size_t written;
  int error;
  return buffer_.Write(value.data(), value.size(), &written, &error) ==
         rtc::SR_SUCCESS;
}

void MemoryLogWriter::Flush() {}

MemoryLogWriterFactory::MemoryLogWriterFactory(MemoryLogWriterManager* manager)
    : manager_(manager) {}

std::unique_ptr<RtcEventLogOutput> MemoryLogWriterFactory::Create(
    std::string filename) {
  return absl::make_unique<MemoryLogWriter>(manager_, filename);
}

MemoryLogWriterFactory::~MemoryLogWriterFactory() {}

}  // namespace webrtc_impl

MemoryLogWriterManager::MemoryLogWriterManager() {}

std::unique_ptr<LogWriterFactoryInterface>
MemoryLogWriterManager::CreateFactory() {
  return absl::make_unique<webrtc_impl::MemoryLogWriterFactory>(this);
}

void MemoryLogWriterManager::Deliver(std::string name,
                                     const rtc::MemoryStream& buffer) {
  size_t size;
  buffer.GetSize(&size);
  finalized_[name] = std::string(buffer.GetBuffer(), size);
}

// namespace webrtc_impl
}  // namespace webrtc
