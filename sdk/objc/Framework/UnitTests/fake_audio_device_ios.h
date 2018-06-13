/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef FAKC_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
#define FAKC_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_

#include "sdk/objc/Framework/Native/src/audio/audio_device_ios.h"

namespace webrtc {

class FakeAudioDeviceIOS : public ios_adm::AudioDeviceIOS {
 public:
  InitStatus Init() override {
    initialized_ = true;
    return InitStatus::OK;
  }

  int32_t Terminate() override {
    initialized_ = false;
    playout_initialized_ = false;
    recording_initialized_ = false;
    return 0;
  }
  bool Initialized() const override { return initialized_; }

  int32_t InitPlayout() override {
    playout_initialized_ = true;
    return 0;
  }
  bool PlayoutIsInitialized() const override { return playout_initialized_; }

  int32_t InitRecording() override {
    recording_initialized_ = true;
    return 0;
  }
  bool RecordingIsInitialized() const override {
    return recording_initialized_;
  }

  int32_t StartPlayout() override {
    RTC_DCHECK(playout_initialized_);
    RTC_DCHECK(!playing_);
    playing_ = true;
    return 0;
  }
  int32_t StopPlayout() override {
    playing_ = false;
    playout_initialized_ = false;
    return 0;
  }
  bool Playing() const override { return playing_; }

  int32_t StartRecording() override {
    RTC_DCHECK(recording_initialized_);
    RTC_DCHECK(!recording_);
    recording_ = true;
    return 0;
  }
  int32_t StopRecording() override {
    recording_ = false;
    recording_initialized_ = false;
    return 0;
  }
  bool Recording() const override { return recording_; }

 private:
  bool recording_;
  bool playing_;
  bool initialized_;
  bool playout_initialized_;
  bool recording_initialized_;
};

}  // namespace webrtc

#endif  // FAKC_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
