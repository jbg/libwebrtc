/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/audio_track_jni.h"
#include "modules/audio_device/android/audio_manager.h"

#include <utility>

#include <android/log.h>

#include "modules/audio_device/generated_voiceengine_jni/jni/WebRtcAudioTrack_jni.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/jni/jni_helpers.h"
#include "rtc_base/platform_thread.h"

#define TAG "AudioTrackJni"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

// AudioTrackJni::JavaAudioTrack implementation.
AudioTrackJni::JavaAudioTrack::JavaAudioTrack(
    const jni::ScopedJavaLocalRef<jobject>& audio_track)
    : env_(audio_track.env()), audio_track_(audio_track) {}

AudioTrackJni::JavaAudioTrack::~JavaAudioTrack() {}

bool AudioTrackJni::JavaAudioTrack::InitPlayout(int sample_rate, int channels) {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_initPlayout(env_, audio_track_, sample_rate,
                                           channels);
}

bool AudioTrackJni::JavaAudioTrack::StartPlayout() {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_startPlayout(env_, audio_track_);
}

bool AudioTrackJni::JavaAudioTrack::StopPlayout() {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_stopPlayout(env_, audio_track_);
}

bool AudioTrackJni::JavaAudioTrack::SetStreamVolume(int volume) {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_setStreamVolume(env_, audio_track_, volume);
}

int AudioTrackJni::JavaAudioTrack::GetStreamMaxVolume() {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_getStreamMaxVolume(env_, audio_track_);
}

int AudioTrackJni::JavaAudioTrack::GetStreamVolume() {
  thread_checker_.CalledOnValidThread();
  return Java_WebRtcAudioTrack_getStreamVolume(env_, audio_track_);
}

// TODO(henrika): possible extend usage of AudioManager and add it as member.
AudioTrackJni::AudioTrackJni(AudioManager* audio_manager)
    : audio_parameters_(audio_manager->GetPlayoutAudioParameters()),
      direct_buffer_address_(nullptr),
      direct_buffer_capacity_in_bytes_(0),
      frames_per_buffer_(0),
      initialized_(false),
      playing_(false),
      audio_device_buffer_(nullptr) {
  ALOGD("ctor[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(audio_parameters_.is_valid());
  j_audio_track_.reset(new JavaAudioTrack(Java_WebRtcAudioTrack_Constructor(
      jni::AttachCurrentThreadIfNeeded(), jni::jlongFromPointer(this))));
  // Detach from this thread since we want to use the checker to verify calls
  // from the Java based audio thread.
  thread_checker_java_.DetachFromThread();
}

AudioTrackJni::~AudioTrackJni() {
  ALOGD("~dtor[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

int32_t AudioTrackJni::Init() {
  ALOGD("Init[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int32_t AudioTrackJni::Terminate() {
  ALOGD("Terminate[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  StopPlayout();
  return 0;
}

int32_t AudioTrackJni::InitPlayout() {
  ALOGD("InitPlayout[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK(!playing_);
  if (!j_audio_track_->InitPlayout(audio_parameters_.sample_rate(),
                                   audio_parameters_.channels())) {
    ALOGE("InitPlayout failed!");
    return -1;
  }
  initialized_ = true;
  return 0;
}

int32_t AudioTrackJni::StartPlayout() {
  ALOGD("StartPlayout[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playing_);
  if (!j_audio_track_->StartPlayout()) {
    ALOGE("StartPlayout failed!");
    return -1;
  }
  playing_ = true;
  return 0;
}

int32_t AudioTrackJni::StopPlayout() {
  ALOGD("StopPlayout[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  if (!j_audio_track_->StopPlayout()) {
    ALOGE("StopPlayout failed!");
    return -1;
  }
  // If we don't detach here, we will hit a RTC_DCHECK in OnDataIsRecorded()
  // next time StartRecording() is called since it will create a new Java
  // thread.
  thread_checker_java_.DetachFromThread();
  initialized_ = false;
  playing_ = false;
  direct_buffer_address_ = nullptr;
  return 0;
}

int AudioTrackJni::SpeakerVolumeIsAvailable(bool& available) {
  available = true;
  return 0;
}

int AudioTrackJni::SetSpeakerVolume(uint32_t volume) {
  ALOGD("SetSpeakerVolume(%d)[tid=%d]", volume, rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return j_audio_track_->SetStreamVolume(volume) ? 0 : -1;
}

int AudioTrackJni::MaxSpeakerVolume(uint32_t& max_volume) const {
  ALOGD("MaxSpeakerVolume[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  max_volume = j_audio_track_->GetStreamMaxVolume();
  return 0;
}

int AudioTrackJni::MinSpeakerVolume(uint32_t& min_volume) const {
  ALOGD("MaxSpeakerVolume[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  min_volume = 0;
  return 0;
}

int AudioTrackJni::SpeakerVolume(uint32_t& volume) const {
  ALOGD("SpeakerVolume[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  volume = j_audio_track_->GetStreamVolume();
  return 0;
}

// TODO(henrika): possibly add stereo support.
void AudioTrackJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  ALOGD("AttachAudioBuffer[tid=%d]", rtc::CurrentThreadId());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  ALOGD("SetPlayoutSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetPlayoutSampleRate(sample_rate_hz);
  const size_t channels = audio_parameters_.channels();
  ALOGD("SetPlayoutChannels(%" PRIuS ")", channels);
  audio_device_buffer_->SetPlayoutChannels(channels);
}

void AudioTrackJni::CacheDirectBufferAddress(
    JNIEnv* env,
    const jni::JavaParamRef<jobject>&,
    const jni::JavaParamRef<jobject>& byte_buffer) {
  ALOGD("OnCacheDirectBufferAddress");
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!direct_buffer_address_);
  direct_buffer_address_ = env->GetDirectBufferAddress(byte_buffer.obj());
  jlong capacity = env->GetDirectBufferCapacity(byte_buffer.obj());
  ALOGD("direct buffer capacity: %lld", capacity);
  direct_buffer_capacity_in_bytes_ = static_cast<size_t>(capacity);
  const size_t bytes_per_frame = audio_parameters_.channels() * sizeof(int16_t);
  frames_per_buffer_ = direct_buffer_capacity_in_bytes_ / bytes_per_frame;
  ALOGD("frames_per_buffer: %" PRIuS, frames_per_buffer_);
}

// This method is called on a high-priority thread from Java. The name of
// the thread is 'AudioRecordTrack'.
void AudioTrackJni::GetPlayoutData(JNIEnv* env,
                                   const jni::JavaParamRef<jobject>&,
                                   size_t length) {
  RTC_DCHECK(thread_checker_java_.CalledOnValidThread());
  const size_t bytes_per_frame = audio_parameters_.channels() * sizeof(int16_t);
  RTC_DCHECK_EQ(frames_per_buffer_, length / bytes_per_frame);
  if (!audio_device_buffer_) {
    ALOGE("AttachAudioBuffer has not been called!");
    return;
  }
  // Pull decoded data (in 16-bit PCM format) from jitter buffer.
  int samples = audio_device_buffer_->RequestPlayoutData(frames_per_buffer_);
  if (samples <= 0) {
    ALOGE("AudioDeviceBuffer::RequestPlayoutData failed!");
    return;
  }
  RTC_DCHECK_EQ(samples, frames_per_buffer_);
  // Copy decoded data into common byte buffer to ensure that it can be
  // written to the Java based audio track.
  samples = audio_device_buffer_->GetPlayoutData(direct_buffer_address_);
  RTC_DCHECK_EQ(length, bytes_per_frame * samples);
}

}  // namespace webrtc
