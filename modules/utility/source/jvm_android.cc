/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/utility/include/jvm_android.h"

#include <android/log.h>

#include "rtc_base/generated_contextutils_jni/jni/ContextUtils_jni.h"
#include "rtc_base/jni/class_loader.h"
#include "rtc_base/jni/jni_helpers.h"
#include "rtc_base/platform_thread.h"

#define TAG "JVM"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace webrtc {

// static
void JVM::Initialize(JavaVM* jvm) {
  ALOGD("JVM::Initialize[tid=%d]", rtc::CurrentThreadId());
  jni::InitClassLoader(jni::AttachCurrentThreadIfNeeded());
}

void JVM::Initialize(JavaVM* jvm, jobject context) {
  ALOGD("JVM::Initialize[tid=%d]", rtc::CurrentThreadId());
  JNIEnv* env = jni::AttachCurrentThreadIfNeeded();
  jni::InitClassLoader(env);
  Java_ContextUtils_initialize(env, jni::JavaParamRef<jobject>(context));
}

}  // namespace webrtc
