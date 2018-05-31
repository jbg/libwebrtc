/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_MODULE_FACTORY_H_
#define MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_MODULE_FACTORY_H_

#include <memory>
#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceModuleFactory {
 public:
  virtual std::unique_ptr<AudioDeviceModule> CreateAudioDeviceModuel() = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_MODULE_FACTORY_H_
