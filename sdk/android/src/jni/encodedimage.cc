/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/encodedimage.h"

#include "sdk/android/generated_video_jni/jni/EncodedImage_jni.h"

namespace webrtc {
namespace jni {

jobject Java_EncodedImage_createFrameType(JNIEnv* env, FrameType frame_type) {
  return ::Java_EncodedImage_createFrameType(env, frame_type);
}

jobject Java_EncodedImage_Create(JNIEnv* env,
                                 jobject buffer,
                                 int encoded_width,
                                 int encoded_height,
                                 int64_t capture_time_ns,
                                 jobject frame_type,
                                 VideoRotation rotation,
                                 bool is_complete_frame,
                                 jobject qp) {
  return ::Java_EncodedImage_Create(
      env, buffer, encoded_width, encoded_height, capture_time_ns, frame_type,
      static_cast<jint>(rotation), is_complete_frame, qp);
}

}  // namespace jni
}  // namespace webrtc
