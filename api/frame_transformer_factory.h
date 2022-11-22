/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FRAME_TRANSFORMER_FACTORY_H_
#define API_FRAME_TRANSFORMER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_frame_metadata.h"

namespace webrtc {

std::unique_ptr<TransformableVideoFrameInterface> CreateVideoSenderFrame();
std::unique_ptr<TransformableVideoFrameInterface> CreateVideoReceiverFrame();
// Creates a new frame with the same metadata as the original.
// The original can be a sender or receiver frame.
std::unique_ptr<TransformableVideoFrameInterface> CloneVideoFrame(
    TransformableVideoFrameInterface* original);
}  // namespace webrtc

#endif  // API_FRAME_TRANSFORMER_FACTORY_H_
