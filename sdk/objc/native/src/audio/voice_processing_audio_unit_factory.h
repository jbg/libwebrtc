/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_FACTORY_H_
#define OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_FACTORY_H_

#include "third_party/webrtc/files/stable/webrtc/api/scoped_refptr.h"
#include "voice_processing_audio_unit.h"

namespace webrtc {
namespace ios_adm {

class VoiceProcessingAudioUnitFactory {
 public:
  virtual VoiceProcessingAudioUnit* CreateVoiceProcessingAudioUnit(
      VoiceProcessingAudioUnitObserver* observer, bool microphone_enabled) = 0;
  virtual ~VoiceProcessingAudioUnitFactory(){};
};

}  // namespace ios_adm
}  // namespace webrtc

#endif  // OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_FACTORY_H_
