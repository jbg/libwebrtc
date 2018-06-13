/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>

#include "rtc_base/refcountedobject.h"
#include "sdk/objc/Framework/Native/src/audio/audio_device_module_ios.h"
#include "sdk/objc/Framework/UnitTests/mock_audio_device_ios.h"

using webrtc::AudioDeviceModule;
using webrtc::MockAudioDeviceIOS;
using webrtc::AudioDeviceGeneric;
using webrtc::ios_adm::AudioDeviceModuleIOS;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;

@interface AudioDeviceModuleIOSTests : NSObject
@end

@implementation AudioDeviceModuleIOSTests

- (void)testCreateAudioDeviceModuleIOS {
  rtc::scoped_refptr<AudioDeviceModule> audio_device_module =
      new rtc::RefCountedObject<AudioDeviceModuleIOS>();
  EXPECT_TRUE(audio_device_module != nullptr);
}

- (void)testCreateAudioDeviceModuleIOSWithCustomAudioDevice {
  MockAudioDeviceIOS *audio_device = new MockAudioDeviceIOS();

  audio_device->DelegateToFake();

  EXPECT_CALL(*audio_device, Init());
  EXPECT_CALL(*audio_device, Initialized()).Times(AnyNumber());

  EXPECT_CALL(*audio_device, InitPlayout());
  EXPECT_CALL(*audio_device, PlayoutIsInitialized()).Times(AnyNumber());

  EXPECT_CALL(*audio_device, InitRecording());
  EXPECT_CALL(*audio_device, RecordingIsInitialized()).Times(AnyNumber());

  EXPECT_CALL(*audio_device, StartPlayout());
  EXPECT_CALL(*audio_device, Playing()).Times(AnyNumber());
  EXPECT_CALL(*audio_device, StopPlayout());

  EXPECT_CALL(*audio_device, StartRecording());
  EXPECT_CALL(*audio_device, Recording()).Times(AnyNumber());
  EXPECT_CALL(*audio_device, StopRecording());

  EXPECT_CALL(*audio_device, Terminate());

  rtc::scoped_refptr<AudioDeviceModule> audio_device_module =
      new rtc::RefCountedObject<AudioDeviceModuleIOS>(
          std::unique_ptr<webrtc::AudioDeviceGeneric>(audio_device));
  EXPECT_TRUE(audio_device_module != nullptr);

  EXPECT_EQ(0, audio_device_module->Init());
  EXPECT_TRUE(audio_device_module->Initialized());

  EXPECT_EQ(0, audio_device_module->InitPlayout());
  EXPECT_TRUE(audio_device_module->PlayoutIsInitialized());

  EXPECT_EQ(0, audio_device_module->InitRecording());
  EXPECT_TRUE(audio_device_module->RecordingIsInitialized());

  EXPECT_EQ(0, audio_device_module->StartPlayout());
  EXPECT_TRUE(audio_device_module->Playing());

  EXPECT_EQ(0, audio_device_module->StartRecording());
  EXPECT_TRUE(audio_device_module->Recording());

  EXPECT_EQ(0, audio_device_module->StopRecording());
  EXPECT_EQ(0, audio_device_module->StopPlayout());

  EXPECT_EQ(0, audio_device_module->Terminate());

  Mock::VerifyAndClear(audio_device);
}

TEST(AudioDeviceModuleIOSTests, CreateAudioDeviceModuleIOSTest) {
  @autoreleasepool {
    AudioDeviceModuleIOSTests *test = [[AudioDeviceModuleIOSTests alloc] init];
    [test testCreateAudioDeviceModuleIOS];
  }
}

TEST(AudioDeviceModuleIOSTests, CreateAudioDeviceModuleIOSWithCustomAudioDeviceTest) {
  @autoreleasepool {
    AudioDeviceModuleIOSTests *test = [[AudioDeviceModuleIOSTests alloc] init];
    [test testCreateAudioDeviceModuleIOSWithCustomAudioDevice];
  }
}

@end
