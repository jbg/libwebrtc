/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/contextutils.h"

#include "rtc_base/generated_base_jni/jni/ContextUtils_jni.h"

namespace webrtc {
namespace jni {

void InitializeContextUtils(JNIEnv* env,
                            JavaRef<jobject> const& application_context) {
  Java_ContextUtils_initialize(env, application_context);
}

}  // namespace jni
}  // namespace webrtc
