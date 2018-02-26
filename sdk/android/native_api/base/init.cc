/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/base/init.h"

#include "rtc_base/checks.h"
#include "sdk/android/native_api/jni/class_loader.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/contextutils.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {

void InitAndroid(JavaVM* jvm) {
  RTC_DCHECK(jvm != nullptr);
  RTC_CHECK_GE(jni::InitGlobalJniVariables(jvm), 0);
  InitClassLoader(jni::GetEnv());
}

void SetApplicationContext(JNIEnv* env, jobject application_context) {
  JavaParamRef<jobject> context_ref(application_context);
  RTC_DCHECK(!context_ref.is_null());
  jni::InitializeContextUtils(env, context_ref);
}

}  // namespace webrtc
