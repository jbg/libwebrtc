/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_ENCODEDIMAGE_H_
#define SDK_ANDROID_SRC_JNI_ENCODEDIMAGE_H_

#include <jni.h>

#include "common_types.h"  // NOLINT(build/include)
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {

class EncodedImage;

namespace jni {

template <>
jobject JavaFromNative(JNIEnv* env, const FrameType& frame_type);
template <>
jobject JavaFromNative(JNIEnv* jni, const EncodedImage& image);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_ENCODEDIMAGE_H_
