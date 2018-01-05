/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpReceiverInterface.
// An RtpReceiver associates a MediaStreamTrackInterface with an underlying
// transport (provided by cricket::VoiceChannel/cricket::VideoChannel)

#ifndef PC_RTPRECEIVER_H_
#define PC_RTPRECEIVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "api/mediastreaminterface.h"
#include "api/rtpreceiverinterface.h"
#include "media/base/videobroadcaster.h"
#include "pc/channel.h"
#include "pc/remoteaudiosource.h"
#include "pc/videotracksource.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Internal class used by PeerConnection.
class RtpReceiverInternal : public RtpReceiverInterface,
                            public sigslot::has_slots<> {
 public:
  virtual void Stop() = 0;
  virtual bool stopped() const = 0;

  // This SSRC is used as an identifier for the receiver between the API layer
  // and the WebRtcVideoEngine, WebRtcVoiceEngine layer.
  uint32_t ssrc() const { return ssrc_; }

  // Partial RtpReceiverInterface implementation.
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams()
      const override;
  std::string id() const override;
  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;
  void SetObserver(RtpReceiverObserverInterface* observer) override;
  std::vector<RtpSource> GetSources() const override;

 protected:
  RtpReceiverInternal(
      rtc::Thread* worker_thread,
      const std::string& id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams,
      uint32_t ssrc,
      cricket::BaseChannel* channel);
  ~RtpReceiverInternal() override;

  virtual RtpParameters GetParameters_w() const = 0;
  virtual bool SetParameters_w(const RtpParameters& parameters) = 0;
  virtual std::vector<RtpSource> GetSources_w() const = 0;

  void SetChannelInternal(cricket::BaseChannel* channel);

  void OnFirstPacketReceived(cricket::BaseChannel* channel);

  rtc::Thread* const worker_thread_;
  const std::string id_;
  const std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_;
  const uint32_t ssrc_;

  cricket::BaseChannel* channel_ = nullptr;
  // |media_channel_| is owned by |channel_|. Should only be accessed on the
  // worker thread.
  cricket::MediaChannel* media_channel_ = nullptr;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
};

class AudioRtpReceiver : public RtpReceiverInternal,
                         public ObserverInterface,
                         public AudioSourceInterface::AudioObserver {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  // TODO(deadbeef): Use rtc::Optional, or have another constructor that
  // doesn't take an SSRC, and make this one DCHECK(ssrc != 0).
  AudioRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
      uint32_t ssrc,
      cricket::VoiceChannel* channel);

  rtc::scoped_refptr<AudioTrackInterface> audio_track() const {
    return track_.get();
  }

  void SetChannel(cricket::VoiceChannel* channel);

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override;
  cricket::MediaType media_type() const override;

  // RtpReceiverInternal implementation.
  void Stop() override;
  bool stopped() const override;

  // ObserverInterface implementation
  void OnChanged() override;

  // AudioSourceInterface::AudioObserver implementation
  void OnSetVolume(double volume) override;

 protected:
  ~AudioRtpReceiver() override;

  // RtpReceiverInternal implementation.
  RtpParameters GetParameters_w() const override;
  bool SetParameters_w(const RtpParameters& parameters) override;
  std::vector<RtpSource> GetSources_w() const override;

 private:
  cricket::VoiceMediaChannel* media_channel() const {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return static_cast<cricket::VoiceMediaChannel*>(media_channel_);
  }

  void Reconfigure();
  bool SetOutputVolume(double volume);

  const rtc::scoped_refptr<AudioTrackInterface> track_;
  bool cached_track_enabled_;
  double cached_volume_ = 1;
  bool stopped_ = false;
};

class VideoRtpReceiver : public RtpReceiverInternal {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  VideoRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
      uint32_t ssrc,
      cricket::VideoChannel* channel);

  rtc::scoped_refptr<VideoTrackInterface> video_track() const {
    return track_.get();
  }

  void SetChannel(cricket::VideoChannel* channel);

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }
  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

  // RtpReceiverInternal implementation.
  void Stop() override;
  bool stopped() const override;

 protected:
  ~VideoRtpReceiver() override;

  // RtpReceiverInternal implementation.
  RtpParameters GetParameters_w() const override;
  bool SetParameters_w(const RtpParameters& parameters) override;
  std::vector<RtpSource> GetSources_w() const override;

 private:
  cricket::VideoChannel* channel() const {
    return static_cast<cricket::VideoChannel*>(channel_);
  }

  cricket::VideoMediaChannel* media_channel() const {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return static_cast<cricket::VideoMediaChannel*>(media_channel_);
  }

  // |broadcaster_| is needed since the decoder can only handle one sink.
  // It might be better if the decoder can handle multiple sinks and consider
  // the VideoSinkWants.
  rtc::VideoBroadcaster broadcaster_;
  // |source_| is held here to be able to change the state of the source when
  // the VideoRtpReceiver is stopped.
  rtc::scoped_refptr<VideoTrackSource> source_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  bool stopped_ = false;
};

}  // namespace webrtc

#endif  // PC_RTPRECEIVER_H_
