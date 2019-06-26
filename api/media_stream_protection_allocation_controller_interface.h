/* Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is an EXPERIMENTAL interface for controlling retransmission.

#ifndef API_MEDIA_STREAM_PROTECTION_ALLOCATION_CONTROLLER_INTERFACE_H_
#define API_MEDIA_STREAM_PROTECTION_ALLOCATION_CONTROLLER_INTERFACE_H_

namespace webrtc {

// Interface for enabling/disabling bitrate allocation for protection (FEC,
// retransmissions) on a media stream.
class MediaStreamProtectionAllocationControllerInterface {
 public:
  virtual void SetProtectionMethod(bool enable_fec, bool enable_nack) = 0;

 protected:
  virtual ~MediaStreamProtectionAllocationControllerInterface() = default;
};

}  // namespace webrtc

#endif  // API_MEDIA_STREAM_PROTECTION_ALLOCATION_CONTROLLER_INTERFACE_H_
