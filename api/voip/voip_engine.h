//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_ENGINE_H_
#define API_VOIP_VOIP_ENGINE_H_

#include <memory>

#include "api/voip/voip_channel.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_network.h"

namespace webrtc {

// VoipEngine interfaces
//
// These pointer interfaces are valid as long as VoipEngine is available.
// Therefore, application must synchronize the usage within the life span of
// created VoipEngine instance.
//
//   auto voip_engine =
//       webrtc::VoipEngineBuilder()
//           .SetAudioEncoderFactory(CreateBuiltinAudioEncoderFactory())
//           .SetAudioDecoderFactory(CreateBuiltinAudioDecoderFactory())
//           .Create();
//
//   auto* voip_channel = voip_engine->ChannelInterface();
//   auto* voip_codec = voip_engine->CodecInterface();
//   auto* voip_network = voip_engine->NetworkInterface();
//
//   VoipChannel::Config config = { &app_transport_, 0xdeadc0de };
//   int channel = voip_channel->CreateChannel(config);
//
//   // After SDP offer/answer, payload type and codec usage have been
//   // decided through negotiation.
//   voip_codec->SetSendCodec(channel, ...);
//   voip_codec->SetReceiveCodecs(channel, ...);
//
//   // Start Send/Playout on voip channel.
//   voip_channel->StartSend(channel);
//   voip_channel->StartPlayout(channel);
//
//   // Inject received rtp/rtcp thru voip network interface.
//   voip_network->ReceivedRTPPacket(channel, rtp_data, rtp_size);
//   voip_network->ReceivedRTCPPacket(channel, rtcp_data, rtcp_size);
//
//   // Stop and release voip channel.
//   voip_channel->StopSend(channel);
//   voip_channel->StopPlayout(channel);
//
//   voip_channel->ReleaseChannel(channel);
//
class VoipEngine {
 public:
  // VoipChannel is the audio session management interface that
  // create/release/start/stop one-to-one audio media session.
  //
  virtual VoipChannel* ChannelInterface() = 0;

  // VoipNetwork provides injection APIs that would enable application
  // to send and receive RTP/RTCP packets. There is no default network module
  // that provides RTP transmission and reception.
  //
  virtual VoipNetwork* NetworkInterface() = 0;

  // VoipCodec provides codec configuration APIs for encoder and decoders.
  virtual VoipCodec* CodecInterface() = 0;

  virtual ~VoipEngine() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_ENGINE_H_
