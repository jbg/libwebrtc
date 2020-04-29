/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_
#define CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_

#include "api/video/test/mock_video_stream_encoder_observer.h"

namespace webrtc {

class FakeFrameRateProvider : public MockVideoStreamEncoderObserver {
 public:
  FakeFrameRateProvider();
  void set_fps(int fps);
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_
