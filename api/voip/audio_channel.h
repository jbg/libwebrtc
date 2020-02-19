//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_AUDIO_CHANNEL_H_
#define API_VOIP_AUDIO_CHANNEL_H_

#include <memory>
#include <queue>

#include "api/call/transport.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/audio_egress.h"
#include "api/voip/audio_ingress.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

class AudioChannel : public Transport {
 public:
  AudioChannel(Clock* clock,
               TaskQueueFactory* task_queue_factory,
               ProcessThread* process_thread,
               AudioMixer* audio_mixer,
               rtc::scoped_refptr<AudioDecoderFactory> decoder_factory);
  ~AudioChannel() override;

  // Debugging purpose.
  void SetChannelId(int channel);

  // Transport interfaces as AudioChannel is intercepting the rtp in/out
  // in the middle for now.
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  AudioEgress* GetAudioEgress();
  AudioIngress* GetAudioIngress();

  bool RegisterTransport(Transport* transport);
  bool DeRegisterTransport();

  void ReceivedRTPPacket(const uint8_t* data, size_t length);
  void ReceivedRTCPPacket(const uint8_t* data, size_t length);

  void StartSend();
  void StopSend();
  void StartPlay();
  void StopPlay();

 private:
  int channel_ = -1;

  Transport* transport_ RTC_GUARDED_BY(lock_) = nullptr;  // not owned
  rtc::CriticalSection lock_;

  AudioMixer* audio_mixer_ = nullptr;
  ProcessThread* process_thread_ = nullptr;

  // owns the common rtp stack instance and used by AudioIngress/Egress
  std::unique_ptr<RtpRtcp> rtp_rtcp_;

  std::unique_ptr<AudioEgress> egress_;
  std::unique_ptr<AudioIngress> ingress_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioChannel);
};

}  // namespace webrtc

#endif  // API_VOIP_AUDIO_CHANNEL_H_
