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

#include <memory>

#include "sdk/objc/Framework/Headers/WebRTC/RTCMacros.h"
#include "sdk/objc/Framework/Native/src/audio/audio_device_ios.h"
#include "test/gmock.h"

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

  MOCK_METHOD0(Die, void());
  ~MockAudioDeviceIOS() { Die(); }
};

}  // namespace webrtc

#endif  // MOCK_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
