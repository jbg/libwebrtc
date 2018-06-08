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

#import <XCTest/XCTest.h>

using webrtc::AudioDeviceModule;
using webrtc::MockAudioDeviceIOS;
using webrtc::AudioDeviceGeneric;
using webrtc::ios_adm::AudioDeviceModuleIOS;
using testing::Mock;
using testing::Return;

@interface AudioDeviceModuleIOSTests : XCTestCase
@end

@implementation AudioDeviceModuleIOSTests

- (void)testCreateAudioDeviceModuleIOS {
  rtc::scoped_refptr<AudioDeviceModule> audio_device_module =
      new rtc::RefCountedObject<AudioDeviceModuleIOS>();
  XCTAssertTrue(audio_device_module != nullptr);
}

- (void)testCreateAudioDeviceModuleIOSWithCustomAudioDevice {
  MockAudioDeviceIOS* audio_device = new MockAudioDeviceIOS();

  rtc::scoped_refptr<AudioDeviceModule> audio_device_module =
      new rtc::RefCountedObject<AudioDeviceModuleIOS>(
          std::unique_ptr<webrtc::AudioDeviceGeneric>(audio_device));
  XCTAssertTrue(audio_device_module != nullptr);

  EXPECT_CALL(*audio_device, Init()).WillOnce(Return(AudioDeviceGeneric::InitStatus::OK));
  EXPECT_CALL(*audio_device, Initialized()).WillOnce(Return(true));

  EXPECT_CALL(*audio_device, InitPlayout()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device, PlayoutIsInitialized()).WillOnce(Return(true));

  EXPECT_CALL(*audio_device, InitRecording()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device, RecordingIsInitialized()).WillOnce(Return(true));

  EXPECT_CALL(*audio_device, StartPlayout()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device, Playing()).WillOnce(Return(true));
  EXPECT_CALL(*audio_device, StopPlayout()).WillOnce(Return(0));

  EXPECT_CALL(*audio_device, StartRecording()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device, Recording()).WillOnce(Return(true));
  EXPECT_CALL(*audio_device, StopRecording()).WillOnce(Return(0));

  EXPECT_CALL(*audio_device, Terminate()).WillOnce(Return(0));

  XCTAssertEqual(0, audio_device_module->Init());
  XCTAssertTrue(audio_device_module->Initialized());

  XCTAssertEqual(0, audio_device_module->InitPlayout());
  XCTAssertTrue(audio_device_module->PlayoutIsInitialized());

  XCTAssertEqual(0, audio_device_module->InitRecording());
  XCTAssertTrue(audio_device_module->RecordingIsInitialized());

  XCTAssertEqual(0, audio_device_module->StartPlayout());
  XCTAssertTrue(audio_device_module->Playing());

  XCTAssertEqual(0, audio_device_module->StartRecording());
  XCTAssertTrue(audio_device_module->Recording());

  XCTAssertEqual(0, audio_device_module->StopRecording());
  XCTAssertEqual(0, audio_device_module->StopPlayout());

  XCTAssertEqual(0, audio_device_module->Terminate());

  Mock::VerifyAndClear(audio_device);
}
@end
