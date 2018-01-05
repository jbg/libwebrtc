/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtpreceiver.h"

#include <utility>
#include <vector>

#include "api/mediastreamtrackproxy.h"
#include "api/videosourceproxy.h"
#include "pc/audiotrack.h"
#include "pc/videotrack.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

RtpReceiverInternal::RtpReceiverInternal(
    rtc::Thread* worker_thread,
    const std::string& id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams,
    uint32_t ssrc,
    cricket::BaseChannel* channel)
    : worker_thread_(worker_thread), id_(id), streams_(streams), ssrc_(ssrc) {
  RTC_DCHECK(worker_thread);
  SetChannelInternal(channel);
}

RtpReceiverInternal::~RtpReceiverInternal() = default;

void RtpReceiverInternal::SetChannelInternal(cricket::BaseChannel* channel) {
  if (channel_) {
    channel_->SignalFirstPacketReceived.disconnect(this);
  }
  channel_ = channel;
  media_channel_ = (channel ? channel->media_channel() : nullptr);
  if (channel_) {
    channel_->SignalFirstPacketReceived.connect(
        this, &RtpReceiverInternal::OnFirstPacketReceived);
  }
}

void RtpReceiverInternal::OnFirstPacketReceived(cricket::BaseChannel* channel) {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

std::vector<rtc::scoped_refptr<MediaStreamInterface>>
RtpReceiverInternal::streams() const {
  return streams_;
}

std::string RtpReceiverInternal::id() const {
  return id_;
}

RtpParameters RtpReceiverInternal::GetParameters() const {
  if (!channel_ || stopped()) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(
      RTC_FROM_HERE, [&] { return GetParameters_w(); });
}

bool RtpReceiverInternal::SetParameters(const RtpParameters& parameters) {
  if (!channel_ || stopped()) {
    return false;
  }
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, [&] { return SetParameters_w(parameters); });
}

void RtpReceiverInternal::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
}

std::vector<RtpSource> RtpReceiverInternal::GetSources() const {
  if (!channel_) {
    return {};
  }
  return worker_thread_->Invoke<std::vector<RtpSource>>(
      RTC_FROM_HERE, [&] { return GetSources_w(); });
}

AudioRtpReceiver::AudioRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
    uint32_t ssrc,
    cricket::VoiceChannel* channel)
    : RtpReceiverInternal(worker_thread, id, streams, ssrc, channel),
      track_(AudioTrackProxy::Create(
          rtc::Thread::Current(),
          AudioTrack::Create(id, RemoteAudioSource::Create(ssrc, channel)))),
      cached_track_enabled_(track_->enabled()) {
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  Reconfigure();
}

AudioRtpReceiver::~AudioRtpReceiver() {
  track_->GetSource()->UnregisterAudioObserver(this);
  track_->UnregisterObserver(this);
  Stop();
}

void AudioRtpReceiver::SetChannel(cricket::VoiceChannel* channel) {
  SetChannelInternal(channel);
}

rtc::scoped_refptr<MediaStreamTrackInterface> AudioRtpReceiver::track() const {
  return track_.get();
}

cricket::MediaType AudioRtpReceiver::media_type() const {
  return cricket::MEDIA_TYPE_AUDIO;
}

RtpParameters AudioRtpReceiver::GetParameters_w() const {
  return media_channel()->GetRtpReceiveParameters(ssrc_);
}

bool AudioRtpReceiver::SetParameters_w(const RtpParameters& parameters) {
  return media_channel()->SetRtpReceiveParameters(ssrc_, parameters);
}

std::vector<RtpSource> AudioRtpReceiver::GetSources_w() const {
  return media_channel()->GetSources(ssrc_);
}

void AudioRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  if (channel_) {
    // Allow that SetOutputVolume fail. This is the normal case when the
    // underlying media channel has already been deleted.
    SetOutputVolume(0.0);
  }
  stopped_ = true;
}

bool AudioRtpReceiver::stopped() const {
  return stopped_;
}

void AudioRtpReceiver::OnChanged() {
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    Reconfigure();
  }
}

void AudioRtpReceiver::OnSetVolume(double volume) {
  RTC_DCHECK_GE(volume, 0);
  RTC_DCHECK_LE(volume, 10);
  cached_volume_ = volume;
  if (!channel_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::OnSetVolume: No audio channel exists.";
    return;
  }
  // When the track is disabled, the volume of the source, which is the
  // corresponding WebRtc Voice Engine channel will be 0. So we do not allow
  // setting the volume to the source when the track is disabled.
  if (!stopped_ && track_->enabled()) {
    bool set_volume_success = SetOutputVolume(cached_volume_);
    RTC_DCHECK(set_volume_success);
  }
}

bool AudioRtpReceiver::SetOutputVolume(double volume) {
  RTC_DCHECK(channel_);
  RTC_DCHECK_GE(volume, 0.0);
  RTC_DCHECK_LE(volume, 10.0);
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel()->SetOutputVolume(ssrc_, volume);
  });
}

void AudioRtpReceiver::Reconfigure() {
  RTC_DCHECK(!stopped_);
  if (!channel_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::Reconfigure: No audio channel exists.";
    return;
  }
  bool set_volume_success =
      SetOutputVolume(track_->enabled() ? cached_volume_ : 0);
  RTC_DCHECK(set_volume_success);
}

VideoRtpReceiver::VideoRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
    uint32_t ssrc,
    cricket::VideoChannel* video_channel)
    : RtpReceiverInternal(worker_thread, id, streams, ssrc, video_channel),
      source_(new rtc::RefCountedObject<VideoTrackSource>(&broadcaster_,
                                                          true /* remote */)),
      track_(VideoTrackProxy::Create(
          rtc::Thread::Current(),
          worker_thread,
          VideoTrack::Create(
              id,
              VideoTrackSourceProxy::Create(rtc::Thread::Current(),
                                            worker_thread,
                                            source_),
              worker_thread))) {
  source_->SetState(MediaSourceInterface::kLive);
}

VideoRtpReceiver::~VideoRtpReceiver() {
  // Since cricket::VideoRenderer is not reference counted,
  // we need to remove it from the channel before we are deleted.
  Stop();
}

void VideoRtpReceiver::SetChannel(cricket::VideoChannel* new_channel) {
  if (channel()) {
    channel()->SetSink(ssrc_, nullptr);
  }
  SetChannelInternal(new_channel);
  if (channel()) {
    bool set_sink_success = channel()->SetSink(ssrc_, &broadcaster_);
    RTC_DCHECK(set_sink_success);
  }
}

RtpParameters VideoRtpReceiver::GetParameters_w() const {
  return media_channel()->GetRtpReceiveParameters(ssrc_);
}

bool VideoRtpReceiver::SetParameters_w(const RtpParameters& parameters) {
  return media_channel()->SetRtpReceiveParameters(ssrc_, parameters);
}

std::vector<RtpSource> VideoRtpReceiver::GetSources_w() const {
  return {};
}

void VideoRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  source_->SetState(MediaSourceInterface::kEnded);
  source_->OnSourceDestroyed();
  if (!channel()) {
    RTC_LOG(LS_WARNING) << "VideoRtpReceiver::Stop: No video channel exists.";
  } else {
    // Allow that SetSink fail. This is the normal case when the underlying
    // media channel has already been deleted.
    channel()->SetSink(ssrc_, nullptr);
  }
  stopped_ = true;
}

bool VideoRtpReceiver::stopped() const {
  return stopped_;
}

}  // namespace webrtc
