/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fakemediaengine.h"

namespace cricket {

FakeBaseEngine::FakeBaseEngine()
    : options_changed_(false), fail_create_channel_(false) {}
void FakeBaseEngine::set_fail_create_channel(bool fail) {
  fail_create_channel_ = fail;
}
void FakeBaseEngine::set_rtp_header_extensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  capabilities_.header_extensions = extensions;
}
void FakeBaseEngine::set_rtp_header_extensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  for (const cricket::RtpHeaderExtension& ext : extensions) {
    RtpExtension webrtc_ext;
    webrtc_ext.uri = ext.uri;
    webrtc_ext.id = ext.id;
    capabilities_.header_extensions.push_back(webrtc_ext);
  }
}

FakeVoiceEngine::FakeVoiceEngine() {
  // Add a fake audio codec. Note that the name must not be "" as there are
  // sanity checks against that.
  codecs_.push_back(AudioCodec(101, "fake_audio_codec", 0, 0, 1));
}
RtpCapabilities FakeVoiceEngine::GetCapabilities() const {
  return capabilities_;
}
void FakeVoiceEngine::Init() {}
rtc::scoped_refptr<webrtc::AudioState> FakeVoiceEngine::GetAudioState() const {
  return rtc::scoped_refptr<webrtc::AudioState>();
}
VoiceMediaChannel* FakeVoiceEngine::CreateChannel(webrtc::Call* call,
                                                  const MediaConfig& config,
                                                  const AudioOptions& options) {
  if (fail_create_channel_) {
    return nullptr;
  }

  FakeVoiceMediaChannel* ch = new FakeVoiceMediaChannel(this, options);
  channels_.push_back(ch);
  return ch;
}
FakeVoiceMediaChannel* FakeVoiceEngine::GetChannel(size_t index) {
  return (channels_.size() > index) ? channels_[index] : NULL;
}
void FakeVoiceEngine::UnregisterChannel(VoiceMediaChannel* channel) {
  channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
}
const std::vector<AudioCodec>& FakeVoiceEngine::send_codecs() const {
  return codecs_;
}
const std::vector<AudioCodec>& FakeVoiceEngine::recv_codecs() const {
  return codecs_;
}
void FakeVoiceEngine::SetCodecs(const std::vector<AudioCodec>& codecs) {
  codecs_ = codecs;
}
int FakeVoiceEngine::GetInputLevel() {
  return 0;
}
bool FakeVoiceEngine::StartAecDump(rtc::PlatformFile file,
                                   int64_t max_size_bytes) {
  return false;
}
void FakeVoiceEngine::StopAecDump() {}
bool FakeVoiceEngine::StartRtcEventLog(rtc::PlatformFile file,
                                       int64_t max_size_bytes) {
  return false;
}
void FakeVoiceEngine::StopRtcEventLog() {}

FakeVideoEngine::FakeVideoEngine() : capture_(false) {
  // Add a fake video codec. Note that the name must not be "" as there are
  // sanity checks against that.
  codecs_.push_back(VideoCodec(0, "fake_video_codec"));
}
RtpCapabilities FakeVideoEngine::GetCapabilities() const {
  return capabilities_;
}
bool FakeVideoEngine::SetOptions(const VideoOptions& options) {
  options_ = options;
  options_changed_ = true;
  return true;
}
VideoMediaChannel* FakeVideoEngine::CreateChannel(webrtc::Call* call,
                                                  const MediaConfig& config,
                                                  const VideoOptions& options) {
  if (fail_create_channel_) {
    return nullptr;
  }

  FakeVideoMediaChannel* ch = new FakeVideoMediaChannel(this, options);
  channels_.emplace_back(ch);
  return ch;
}
FakeVideoMediaChannel* FakeVideoEngine::GetChannel(size_t index) {
  return (channels_.size() > index) ? channels_[index] : nullptr;
}
void FakeVideoEngine::UnregisterChannel(VideoMediaChannel* channel) {
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  RTC_DCHECK(it != channels_.end());
  channels_.erase(it);
}
std::vector<VideoCodec> FakeVideoEngine::codecs() const {
  return codecs_;
}
void FakeVideoEngine::SetCodecs(const std::vector<VideoCodec> codecs) {
  codecs_ = codecs;
}
bool FakeVideoEngine::SetCapture(bool capture) {
  capture_ = capture;
  return true;
}

FakeMediaEngine::FakeMediaEngine()
    : CompositeMediaEngine(absl::make_unique<FakeVoiceEngine>(),
                           absl::make_unique<FakeVideoEngine>()),
      voice_(reinterpret_cast<FakeVoiceEngine*>(&voice())),
      video_(reinterpret_cast<FakeVideoEngine*>(&video())) {}
FakeMediaEngine::~FakeMediaEngine() {}
void FakeMediaEngine::SetAudioCodecs(const std::vector<AudioCodec>& codecs) {
  voice_->SetCodecs(codecs);
}
void FakeMediaEngine::SetVideoCodecs(const std::vector<VideoCodec>& codecs) {
  video_->SetCodecs(codecs);
}
void FakeMediaEngine::SetAudioRtpHeaderExtensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  voice_->set_rtp_header_extensions(extensions);
}
void FakeMediaEngine::SetVideoRtpHeaderExtensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  video_->set_rtp_header_extensions(extensions);
}
void FakeMediaEngine::SetAudioRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  voice_->set_rtp_header_extensions(extensions);
}
void FakeMediaEngine::SetVideoRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  video_->set_rtp_header_extensions(extensions);
}
FakeVoiceMediaChannel* FakeMediaEngine::GetVoiceChannel(size_t index) {
  return voice_->GetChannel(index);
}
FakeVideoMediaChannel* FakeMediaEngine::GetVideoChannel(size_t index) {
  return video_->GetChannel(index);
}

bool FakeMediaEngine::capture() const {
  return video_->capture_;
}
bool FakeMediaEngine::options_changed() const {
  return video_->options_changed_;
}
void FakeMediaEngine::clear_options_changed() {
  video_->options_changed_ = false;
}
void FakeMediaEngine::set_fail_create_channel(bool fail) {
  voice_->set_fail_create_channel(fail);
  video_->set_fail_create_channel(fail);
}

DataMediaChannel* FakeDataEngine::CreateChannel(const MediaConfig& config) {
  FakeDataMediaChannel* ch = new FakeDataMediaChannel(this, DataOptions());
  channels_.push_back(ch);
  return ch;
}
FakeDataMediaChannel* FakeDataEngine::GetChannel(size_t index) {
  return (channels_.size() > index) ? channels_[index] : NULL;
}
void FakeDataEngine::UnregisterChannel(DataMediaChannel* channel) {
  channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
}
void FakeDataEngine::SetDataCodecs(const std::vector<DataCodec>& data_codecs) {
  data_codecs_ = data_codecs;
}
const std::vector<DataCodec>& FakeDataEngine::data_codecs() {
  return data_codecs_;
}

}  // namespace cricket
