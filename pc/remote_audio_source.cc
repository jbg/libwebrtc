/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/remote_audio_source.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/trace_event.h"

namespace webrtc {
namespace {

// Returns true if `timestamp` is older than `current_timestamp`.
bool IsOutOfOrderTimestamp(uint32_t current_timestamp, uint32_t timestamp) {
  constexpr uint32_t kMaxDelta = std::numeric_limits<uint32_t>::max() / 2;
  uint32_t delta;
  if (timestamp > current_timestamp) {
    // Most often the delta will be in the order of tens of ms. A corner case to
    // be aware of is if current_timestamp recently wrapped around, then a
    // late-arriving packet could meet this condition but a large delta will
    // catch out of order packets.
    delta = timestamp - current_timestamp;
  } else {
    // In the normal case when `timestamp` wrapped around, the delta will
    // be smaller than `kMaxDelta`. If the delta is larger, then that's an
    // out of order packet.
    delta =
        timestamp + (std::numeric_limits<uint32_t>::max() - current_timestamp);
  }

  return delta > kMaxDelta;
}

}  // namespace

// This proxy is passed to the underlying media engine to receive audio data as
// they come in. The data will then be passed back up to the RemoteAudioSource
// which will fan it out to all the sinks that have been added to it.
class RemoteAudioSource::AudioDataProxy : public AudioSinkInterface {
 public:
  explicit AudioDataProxy(RemoteAudioSource* source) : source_(source) {
    RTC_DCHECK(source);
  }

  AudioDataProxy() = delete;
  AudioDataProxy(const AudioDataProxy&) = delete;
  AudioDataProxy& operator=(const AudioDataProxy&) = delete;

  ~AudioDataProxy() override { source_->OnAudioChannelGone(); }

  // AudioSinkInterface implementation.
  void OnData(const AudioSinkInterface::Data& audio) override {
    source_->OnData(audio);
  }

 private:
  const rtc::scoped_refptr<RemoteAudioSource> source_;
};

RemoteAudioSource::RemoteAudioSource(
    TaskQueueBase* worker_thread,
    OnAudioChannelGoneAction on_audio_channel_gone_action)
    : main_thread_(TaskQueueBase::Current()),
      worker_thread_(worker_thread),
      on_audio_channel_gone_action_(on_audio_channel_gone_action),
      state_(MediaSourceInterface::kInitializing) {
  RTC_DCHECK(main_thread_);
  RTC_DCHECK(worker_thread_);
}

RemoteAudioSource::~RemoteAudioSource() {
  RTC_DCHECK(audio_observers_.empty());
  if (!sinks_.empty()) {
    RTC_LOG(LS_WARNING)
        << "RemoteAudioSource destroyed while sinks_ is non-empty.";
  }
}

void RemoteAudioSource::Start(
    cricket::VoiceMediaReceiveChannelInterface* media_channel,
    absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  // Register for callbacks immediately before AddSink so that we always get
  // notified when a channel goes out of scope (signaled when "AudioDataProxy"
  // is destroyed).
  auto proxy = std::make_unique<AudioDataProxy>(this);
  ssrc ? media_channel->SetRawAudioSink(*ssrc, std::move(proxy))
       : media_channel->SetDefaultRawAudioSink(std::move(proxy));

  media_channel->SetAudioLevelCallback(
      ssrc, [this](uint32_t timestamp, absl::optional<uint8_t> level) {
        RTC_DCHECK_RUN_ON(worker_thread_);
        if (muted_on_worker_thread_ != kAudioStateUnknown) {
          if (IsOutOfOrderTimestamp(rtp_timestamp_, timestamp)) {
            RTC_DLOG(LS_INFO)
                << "Ignoring out of order packet. have=" << rtp_timestamp_
                << " incoming=" << timestamp;
            return;
          }
        }

        rtp_timestamp_ = timestamp;

        AudioStateOnWorker muted = (!level.has_value() || level.value() == 127)
                                       ? kAudioStateMuted
                                       : kAudioStateUnmuted;
        if (muted == muted_on_worker_thread_)
          return;

        muted_on_worker_thread_ = muted;
        main_thread_->PostTask(
            [this, thiz = rtc::scoped_refptr<RemoteAudioSource>(this), muted] {
              RTC_DCHECK_RUN_ON(main_thread_);
              const auto required_current_state =
                  (muted == kAudioStateMuted) ? kLive : kMuted;
              if (state_ == required_current_state)
                SetState((muted == kAudioStateMuted) ? kMuted : kLive);
            });
      });
}

void RemoteAudioSource::Stop(
    cricket::VoiceMediaReceiveChannelInterface* media_channel,
    absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(media_channel);
  ssrc ? media_channel->SetRawAudioSink(*ssrc, nullptr)
       : media_channel->SetDefaultRawAudioSink(nullptr);
  media_channel->SetAudioLevelCallback(ssrc, nullptr);
}

void RemoteAudioSource::SetState(SourceState new_state) {
  RTC_DCHECK_RUN_ON(main_thread_);
  if (state_ != new_state) {
    state_ = new_state;
    FireOnChanged();
  }
}

MediaSourceInterface::SourceState RemoteAudioSource::state() const {
  RTC_DCHECK_RUN_ON(main_thread_);
  return state_;
}

bool RemoteAudioSource::remote() const {
  RTC_DCHECK_RUN_ON(main_thread_);
  return true;
}

void RemoteAudioSource::SetVolume(double volume) {
  RTC_DCHECK_GE(volume, 0);
  RTC_DCHECK_LE(volume, 10);
  RTC_LOG(LS_INFO) << rtc::StringFormat("RAS::%s({volume=%.2f})", __func__,
                                        volume);
  for (auto* observer : audio_observers_) {
    observer->OnSetVolume(volume);
  }
}

void RemoteAudioSource::RegisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  RTC_DCHECK(!absl::c_linear_search(audio_observers_, observer));
  audio_observers_.push_back(observer);
}

void RemoteAudioSource::UnregisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  audio_observers_.remove(observer);
}

void RemoteAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(main_thread_);
  RTC_DCHECK(sink);

  MutexLock lock(&sink_lock_);
  RTC_DCHECK(!absl::c_linear_search(sinks_, sink));
  sinks_.push_back(sink);
}

void RemoteAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(main_thread_);
  RTC_DCHECK(sink);

  MutexLock lock(&sink_lock_);
  sinks_.remove(sink);
}

void RemoteAudioSource::OnData(const AudioSinkInterface::Data& audio) {
  // Called on the externally-owned audio callback thread, via/from webrtc.
  TRACE_EVENT0("webrtc", "RemoteAudioSource::OnData");
  MutexLock lock(&sink_lock_);
  for (auto* sink : sinks_) {
    // When peerconnection acts as an audio source, it should not provide
    // absolute capture timestamp.
    sink->OnData(audio.data, 16, audio.sample_rate, audio.channels,
                 audio.samples_per_channel,
                 /*absolute_capture_timestamp_ms=*/absl::nullopt);
  }
}

void RemoteAudioSource::OnAudioChannelGone() {
  if (on_audio_channel_gone_action_ != OnAudioChannelGoneAction::kEnd) {
    return;
  }
  // Called when the audio channel is deleted. It may be the worker thread or
  // may be a different task queue.
  // This object needs to live long enough for the cleanup logic in the posted
  // task to run, so take a reference to it. Sometimes the task may not be
  // processed (because the task queue was destroyed shortly after this call),
  // but that is fine because the task queue destructor will take care of
  // destroying task which will release the reference on RemoteAudioSource.
  rtc::scoped_refptr<RemoteAudioSource> thiz(this);
  main_thread_->PostTask([thiz = std::move(thiz)] {
    thiz->sinks_.clear();
    thiz->SetState(MediaSourceInterface::kEnded);
  });
}

}  // namespace webrtc
