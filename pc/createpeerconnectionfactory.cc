/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/call/callfactoryinterface.h"
#include "api/peerconnectioninterface.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#if defined(USE_BUILTIN_SW_CODECS)
#include "media/engine/convert_legacy_video_factory.h"  // nogncheck
#endif
#include "media/engine/webrtcmediaengine.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_coding/fec_controller_default.h"
#include "rtc_base/bind.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"

namespace webrtc {

// All the CreatePeerConnectionFactory overloads below depend on built-in
// software video codecs, indirectly through
// "ConvertVideoEncoderFactory"/ConvertVideoDecoderFactory".
#if defined(USE_BUILTIN_SW_CODECS)
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory) {
  return CreatePeerConnectionFactory(nullptr, nullptr, nullptr, nullptr,
                                     audio_encoder_factory,
                                     audio_decoder_factory, nullptr, nullptr,
                                     nullptr, nullptr, nullptr, nullptr);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, audio_processing, nullptr, nullptr);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, nullptr, nullptr, nullptr);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, nullptr, nullptr, nullptr, nullptr);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory) {
  RTC_DCHECK(audio_encoder_factory);
  RTC_DCHECK(audio_decoder_factory);

  PeerConnectionFactoryDependencies deps =
      PeerConnectionFactoryDependencies::Create();
  deps.network_thread = network_thread;
  deps.worker_thread = worker_thread;
  deps.signaling_thread = signaling_thread;
  if (default_adm) {
    deps.audio_device_module = default_adm;
  }
  deps.audio_encoder_factory = audio_encoder_factory;
  deps.audio_decoder_factory = audio_decoder_factory;
  deps.video_encoder_factory = cricket::ConvertVideoEncoderFactory(
      rtc::WrapUnique(video_encoder_factory));
  deps.video_decoder_factory = cricket::ConvertVideoDecoderFactory(
      rtc::WrapUnique(video_decoder_factory));
  if (audio_mixer) {
    deps.audio_mixer = audio_mixer;
  }
  if (audio_processing) {
    deps.audio_processing = audio_processing;
  }
  if (fec_controller_factory) {
    deps.fec_controller_factory = std::move(fec_controller_factory);
  }
  if (network_controller_factory) {
    deps.network_controller_factory = std::move(network_controller_factory);
  }
  return CreatePeerConnectionFactory(std::move(deps));
}

#endif  // defined(USE_BUILTIN_SW_CODECS)

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing) {
  RTC_DCHECK(audio_encoder_factory);
  RTC_DCHECK(audio_decoder_factory);

  PeerConnectionFactoryDependencies deps =
      PeerConnectionFactoryDependencies::Create();
  deps.network_thread = network_thread;
  deps.worker_thread = worker_thread;
  deps.signaling_thread = signaling_thread;
  if (default_adm) {
    deps.audio_device_module = default_adm;
  }
  deps.audio_encoder_factory = audio_encoder_factory;
  deps.audio_decoder_factory = audio_decoder_factory;
  deps.video_encoder_factory = std::move(video_encoder_factory);
  deps.video_decoder_factory = std::move(video_decoder_factory);
  if (audio_mixer) {
    deps.audio_mixer = audio_mixer;
  }
  if (audio_processing) {
    deps.audio_processing = audio_processing;
  }
  return CreatePeerConnectionFactory(std::move(deps));
}

// Every other CreatePeerConnectionFactory overload should end up calling this,
// ultimately.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    PeerConnectionFactoryDependencies deps) {
  std::unique_ptr<cricket::MediaEngineInterface> media_engine(
      cricket::WebRtcMediaEngineFactory::Create(
          deps.audio_device_module, deps.audio_encoder_factory,
          deps.audio_decoder_factory, std::move(deps.video_encoder_factory),
          std::move(deps.video_decoder_factory), deps.audio_mixer,
          deps.audio_processing));

  std::unique_ptr<CallFactoryInterface> call_factory = CreateCallFactory();

  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory =
      CreateRtcEventLogFactory();

  return CreateModularPeerConnectionFactory(
      deps.network_thread, deps.worker_thread, deps.signaling_thread,
      std::move(media_engine), std::move(call_factory),
      std::move(event_log_factory), std::move(deps.fec_controller_factory),
      std::move(deps.network_controller_factory));
}

// static
PeerConnectionFactoryDependencies PeerConnectionFactoryDependencies::Create() {
  PeerConnectionFactoryDependencies deps;
#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)
  deps.audio_device_module = webrtc::AudioDeviceModule::Create(
      webrtc::AudioDeviceModule::kPlatformDefaultAudio);
#endif  // WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE
  deps.audio_mixer = webrtc::AudioMixerImpl::Create();
  deps.audio_processing = AudioProcessingBuilder().Create();
  deps.fec_controller_factory = rtc::MakeUnique<DefaultFecControllerFactory>();
  return deps;
}

}  // namespace webrtc
