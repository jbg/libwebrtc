//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_CHANNEL_H_
#define API_VOIP_VOIP_CHANNEL_H_

#include "api/call/transport.h"

namespace webrtc {

class VoipChannel {
 public:
  struct Config {
    webrtc::Transport* transport = nullptr;
    uint32_t local_ssrc = 0;
  };

  // Create a channel handle in non-negative value including 0.
  // When -1 returned, it represents error during audio channel construction.
  // Each |channel| id maps into one audio media session where each has its
  // own separate module for send/receive rtp packet with one peer.
  virtual int CreateChannel(const Config& config) = 0;

  // Release |channel| that has served the purpose. Returned channel id
  // will be allocated to logic that create channel afterward. Invoking
  // operation on released channel will lead to undefined behavior.
  virtual bool ReleaseChannel(int channel) = 0;

  // Start sending on |channel|. This will start microphone if first to start.
  virtual bool StartSend(int channel) = 0;

  // Stop sending on |channel|. If this is the last active channel, it will
  // stop microphone input from underlying audio platform layer.
  virtual bool StopSend(int channel) = 0;

  // Start playing on speaker device for |channel|.
  // This will start underlying platform speaker device if not started.
  virtual bool StartPlayout(int channel) = 0;

  // Stop playing on speaker device for |channel|. If this is the last
  // active channel playing, then it will stop speaker from the platform layer.
  virtual bool StopPlayout(int channel) = 0;

 protected:
  virtual ~VoipChannel() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_CHANNEL_H_
