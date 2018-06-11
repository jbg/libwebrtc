/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MOCK_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
#define MOCK_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_

#include "sdk/objc/Framework/UnitTests/fake_audio_device_ios.h"
#include "test/gmock.h"

using testing::Invoke;

namespace webrtc {

class MockAudioDeviceIOS : public ios_adm::AudioDeviceIOS {
 public:
  MOCK_METHOD0(Init, InitStatus());
  MOCK_METHOD0(Terminate, int32_t());
  MOCK_CONST_METHOD0(Initialized, bool());

  MOCK_METHOD0(InitPlayout, int32_t());
  MOCK_CONST_METHOD0(PlayoutIsInitialized, bool());

  MOCK_METHOD0(InitRecording, int32_t());
  MOCK_CONST_METHOD0(RecordingIsInitialized, bool());

  MOCK_METHOD0(StartPlayout, int32_t());
  MOCK_METHOD0(StopPlayout, int32_t());
  MOCK_CONST_METHOD0(Playing, bool());

  MOCK_METHOD0(StartRecording, int32_t());
  MOCK_METHOD0(StopRecording, int32_t());
  MOCK_CONST_METHOD0(Recording, bool());

  MOCK_CONST_METHOD1(GetPlayoutAudioParameters, int(AudioParameters* params));
  MOCK_CONST_METHOD1(GetRecordAudioParameters, int(AudioParameters* params));

  // Delegates the default actions of the methods to a FakeAudioDeviceIOS
  // object. This must be called *before* the custom ON_CALL() statements.
  void DelegateToFake() {
    ON_CALL(*this, Init())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::Init));
    ON_CALL(*this, Terminate())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::Terminate));
    ON_CALL(*this, Initialized())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::Initialized));

    ON_CALL(*this, InitPlayout())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::InitPlayout));
    ON_CALL(*this, PlayoutIsInitialized())
        .WillByDefault(
            Invoke(&fake_, &FakeAudioDeviceIOS::PlayoutIsInitialized));

    ON_CALL(*this, InitRecording())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::InitRecording));
    ON_CALL(*this, RecordingIsInitialized())
        .WillByDefault(
            Invoke(&fake_, &FakeAudioDeviceIOS::RecordingIsInitialized));

    ON_CALL(*this, StartPlayout())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::StartPlayout));
    ON_CALL(*this, StopPlayout())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::StopPlayout));
    ON_CALL(*this, Playing())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::Playing));

    ON_CALL(*this, StartRecording())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::StartRecording));
    ON_CALL(*this, StopRecording())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::StopRecording));
    ON_CALL(*this, Recording())
        .WillByDefault(Invoke(&fake_, &FakeAudioDeviceIOS::Recording));
  }

 private:
  FakeAudioDeviceIOS fake_;  // Keeps an instance of the fake in the mock.
};

}  // namespace webrtc

#endif  // MOCK_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
