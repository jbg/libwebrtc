/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_CREATE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_CREATE_H_

#include <memory>

#include "api/video/video_codec_type.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer.h"

namespace webrtc {

std::unique_ptr<RtpVideoDepacketizer> RtpVideoDepacketizerCreate(
    VideoCodecType codec);

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_CREATE_H_
