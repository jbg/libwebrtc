/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains codec dependent definitions that are needed in
// order to compile the WebRTC codebase, even if this codec is not used.

#ifndef MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_GLOBALS_H_
#define MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_GLOBALS_H_

#include "modules/video_coding/codecs/h264/include/h264_globals.h"

namespace webrtc {

struct H265NaluInfo {
  uint8_t type;
  int vps_id;
  int sps_id;
  int pps_id;
};

struct RTPVideoHeaderH265 {
  // The NAL unit type. If this is a header for a fragmented packet, it's the
  // NAL unit type of the original data. If this is the header for an aggregated
  // packet, it's the NAL unit type of the first NAL unit in the packet.
  uint8_t nalu_type;
  H265NaluInfo nalus[kMaxNalusPerPacket];
  size_t nalus_length;
  // Running cuter for every frame to determin frame decodable
  // depending along with Temporal ID (obtained from RTP header extn).
  // '0' if PictureID does not exist.
  uint16_t picture_id;
  // For support slice-based transmission, mark end of a frame so that
  // the H.265 packetizer will not set marker bit for the last fragment of
  // current outgoing data if it does not contain last fragment of the frame;
  // and will treat the first fragment of the frame as continuous playload, so
  // that it will not create FU header or STAP-A header on first fragment if
  // contains last fragment of the frame.
  bool has_last_fragement;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_GLOBALS_H_
