/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)
#include "audio_device_module.h"

#include "api/make_ref_counted.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<ios_adm::AudioDeviceModuleIOS> CreateAudioDeviceModule(
    bool bypass_voice_processing) {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  return rtc::make_ref_counted<ios_adm::AudioDeviceModuleIOS>(bypass_voice_processing);
}
}
#endif
