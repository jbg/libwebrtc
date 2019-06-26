/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_

#include "api/fec_controller_override.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

struct VideoStreamEncoderSettings {
  VideoStreamEncoderSettings(FecControllerOverride* fec_controller_override,
                             const VideoEncoder::Capabilities& capabilities)
      : fec_controller_override(fec_controller_override),
        capabilities(capabilities) {}

  // Enables the new method to estimate the cpu load from encoding, used for
  // cpu adaptation.
  bool experiment_cpu_load_estimator = false;

  // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
  VideoEncoderFactory* encoder_factory = nullptr;

  // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
  VideoBitrateAllocatorFactory* bitrate_allocator_factory = nullptr;

  // TODO: !!!
  const FecControllerOverride* fec_controller_override;

  // Negotiated capabilities which the VideoEncoder may expect the other
  // side to use.
  VideoEncoder::Capabilities capabilities;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
