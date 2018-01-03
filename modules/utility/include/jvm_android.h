/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_JVM_ANDROID_H_
#define MODULES_UTILITY_INCLUDE_JVM_ANDROID_H_

#include <jni.h>

#include "rtc_base/constructormagic.h"

namespace webrtc {

class JVM {
 public:
  // Stores global handles to the Java VM interface.
  // Should be called once on a thread that is attached to the JVM.
  static void Initialize(JavaVM* jvm);
  // Like the method above but also passes the context to the ContextUtils
  // class. This method should be used by pure-C++ Android users that can't call
  // ContextUtils.initialize directly.
  static void Initialize(JavaVM* jvm, jobject context);

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(JVM);
};

}  // namespace webrtc

#endif  // MODULES_UTILITY_INCLUDE_JVM_ANDROID_H_
