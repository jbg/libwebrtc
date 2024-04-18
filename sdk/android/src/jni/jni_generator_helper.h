/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Do not include this file directly. It's intended to be used only by the JNI
// generation script. We are exporting types in strange namespaces in order to
// be compatible with the generated code targeted for Chromium.

#ifndef SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
#define SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_

#include <jni.h>

#include <atomic>

#include "third_party/jni_zero/jni_zero_internal.h"

#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))

#if defined(WEBRTC_ARCH_X86)
// Dalvik JIT generated code doesn't guarantee 16-byte stack alignment on
// x86 - use force_align_arg_pointer to realign the stack at the JNI
// boundary. crbug.com/655248
#define JNI_GENERATOR_EXPORT \
  __attribute__((force_align_arg_pointer)) extern "C" JNIEXPORT JNICALL
#else
#define JNI_GENERATOR_EXPORT extern "C" JNIEXPORT JNICALL
#endif

namespace webrtc {
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;
}  // namespace webrtc


// Re-export helpers in the old jni_generator namespace.
// TODO(b/319078685): Remove once all uses of the jni_generator has been
// updated.
namespace jni_generator {
using jni_zero::JniJavaCallContext;
using jni_zero::JniJavaCallContextChecked;
using jni_zero::JniJavaCallContextUnchecked;
}  // namespace jni_generator

// Re-export helpers in the namespaces that the old jni_generator script
// expects.
// TODO(b/319078685): Remove once all uses of the jni_generator has been
// updated.
namespace base {
namespace android {
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::MethodID;
using jni_zero::ScopedJavaLocalRef;
using jni_zero::internal::LazyGetClass;
}  // namespace android
}  // namespace base
#endif  // SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
