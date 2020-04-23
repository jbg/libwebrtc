/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_MEDIA_MEDIA_DUMP_MANAGER_H_
#define TEST_PC_E2E_MEDIA_MEDIA_DUMP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/test/peerconnection_quality_test_fixture.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class MediaDumpManager {
 public:
  ~MediaDumpManager();

  // Creates a video file writer if |file_name| is not empty. Created writer
  // will be owned by MediaHelper and will be closed on MediaHelper destruction
  // or when CloseWriters() will be invoked.
  // If |file_name| is empty will return nullptr.
  test::VideoFrameWriter* MaybeCreateVideoWriter(
      absl::optional<std::string> file_name,
      const PeerConnectionE2EQualityTestFixture::VideoConfig& config);

  void CloseWriters();

 private:
  std::vector<std::unique_ptr<test::VideoFrameWriter>> video_writers_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_MEDIA_MEDIA_DUMP_MANAGER_H_
