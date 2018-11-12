/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive_proxy.h"

#include <utility>

#include "api/call/audio_sink.h"
#include "audio/channel_send_proxy.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace voe {
ChannelReceiveProxy::ChannelReceiveProxy() {}

ChannelReceiveProxy::ChannelReceiveProxy(
    std::unique_ptr<ChannelReceive> channel)
    : channel_(std::move(channel)) {
  RTC_DCHECK(channel_);
}

ChannelReceiveProxy::~ChannelReceiveProxy() {}

void ChannelReceiveProxy::SetLocalSSRC(uint32_t ssrc) {
  channel_->SetLocalSSRC(ssrc);
}

void ChannelReceiveProxy::SetNACKStatus(bool enable, int max_packets) {
  channel_->SetNACKStatus(enable, max_packets);
}

CallReceiveStatistics ChannelReceiveProxy::GetRTCPStatistics() const {
  return channel_->GetRTCPStatistics();
}

bool ChannelReceiveProxy::ReceivedRTCPPacket(const uint8_t* packet,
                                             size_t length) {
  // May be called on either worker thread or network thread.
  return channel_->ReceivedRTCPPacket(packet, length) == 0;
}

void ChannelReceiveProxy::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  channel_->RegisterReceiverCongestionControlObjects(packet_router);
}

void ChannelReceiveProxy::ResetReceiverCongestionControlObjects() {
  channel_->ResetReceiverCongestionControlObjects();
}

NetworkStatistics ChannelReceiveProxy::GetNetworkStatistics() const {
  return channel_->GetNetworkStatistics();
}

AudioDecodingCallStats ChannelReceiveProxy::GetDecodingCallStatistics() const {
  return channel_->GetDecodingCallStatistics();
}

int ChannelReceiveProxy::GetSpeechOutputLevelFullRange() const {
  return channel_->GetSpeechOutputLevelFullRange();
}

double ChannelReceiveProxy::GetTotalOutputEnergy() const {
  return channel_->GetTotalOutputEnergy();
}

double ChannelReceiveProxy::GetTotalOutputDuration() const {
  return channel_->GetTotalOutputDuration();
}

uint32_t ChannelReceiveProxy::GetDelayEstimate() const {
  return channel_->GetDelayEstimate();
}

void ChannelReceiveProxy::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  channel_->SetReceiveCodecs(codecs);
}

void ChannelReceiveProxy::SetSink(AudioSinkInterface* sink) {
  channel_->SetSink(sink);
}

void ChannelReceiveProxy::OnRtpPacket(const RtpPacketReceived& packet) {
  // May be called on either worker thread or network thread.
  channel_->OnRtpPacket(packet);
}

void ChannelReceiveProxy::SetChannelOutputVolumeScaling(float scaling) {
  channel_->SetChannelOutputVolumeScaling(scaling);
}

AudioMixer::Source::AudioFrameInfo ChannelReceiveProxy::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel_->GetAudioFrameWithInfo(sample_rate_hz, audio_frame);
}

int ChannelReceiveProxy::PreferredSampleRate() const {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel_->PreferredSampleRate();
}

void ChannelReceiveProxy::AssociateSendChannel(
    const ChannelSendProxy& send_channel_proxy) {
  channel_->SetAssociatedSendChannel(send_channel_proxy.GetChannel());
}

void ChannelReceiveProxy::DisassociateSendChannel() {
  channel_->SetAssociatedSendChannel(nullptr);
}

absl::optional<Syncable::Info> ChannelReceiveProxy::GetSyncInfo() const {
  return channel_->GetSyncInfo();
}

uint32_t ChannelReceiveProxy::GetPlayoutTimestamp() const {
  RTC_DCHECK_RUNS_SERIALIZED(&video_capture_thread_race_checker_);
  return channel_->GetPlayoutTimestamp();
}

void ChannelReceiveProxy::SetMinimumPlayoutDelay(int delay_ms) {
  channel_->SetMinimumPlayoutDelay(delay_ms);
}

bool ChannelReceiveProxy::GetRecCodec(CodecInst* codec_inst) const {
  return channel_->GetRecCodec(codec_inst);
}

std::vector<RtpSource> ChannelReceiveProxy::GetSources() const {
  return channel_->GetSources();
}

void ChannelReceiveProxy::StartPlayout() {
  channel_->StartPlayout();
}

void ChannelReceiveProxy::StopPlayout() {
  channel_->StopPlayout();
}
}  // namespace voe
}  // namespace webrtc
