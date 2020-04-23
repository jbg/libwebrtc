/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/media/media_dump_manager.h"

#include <utility>

namespace webrtc {
namespace webrtc_pc_e2e {

MediaDumpManager::~MediaDumpManager() {
  CloseWriters();
}

test::VideoFrameWriter* MediaDumpManager::MaybeCreateVideoWriter(
    absl::optional<std::string> file_name,
    const PeerConnectionE2EQualityTestFixture::VideoConfig& config) {
  if (!file_name.has_value()) {
    return nullptr;
  }
  // TODO(titovartem) create only one file writer for simulcast video track.
  // For now this code will be invoked for each simulcast stream separately, but
  // only one file will be used.
  auto video_writer = std::make_unique<test::Y4mVideoFrameWriterImpl>(
      file_name.value(), config.width, config.height, config.fps);
  test::VideoFrameWriter* out = video_writer.get();
  video_writers_.push_back(std::move(video_writer));
  return out;
}

void MediaDumpManager::CloseWriters() {
  for (const auto& video_writer : video_writers_) {
    video_writer->Close();
  }
  video_writers_.clear();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
