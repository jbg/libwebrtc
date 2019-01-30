/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "sdk/android/generated_native_unittests_jni/jni/JavaTypesTestHelper_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
TEST(JavaTypesTest, TestJavaToNativeStringMap) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_map =
      jni::Java_JavaTypesTestHelper_createTestStringMap(env);

  std::map<std::string, std::string> output = JavaToNativeStringMap(env, j_map);

  std::map<std::string, std::string> expected{
      {"one", "1"}, {"two", "2"}, {"three", "3"},
  };
  EXPECT_EQ(expected, output);
}

TEST(JavaTypesTest, TestJavaArrayRef) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  ScopedJavaLocalRef<jintArray> array;

  {
    JavaIntArrayWritableRef writable = NewJavaIntArray(env, 3);
    writable[0] = 1;
    writable[1] = 20;
    writable[2] = 300;

    array = writable.jarray();
  }

  JavaIntArrayReadableRef readable(env, array);
  EXPECT_EQ(1, readable[0]);
  EXPECT_EQ(20, readable[1]);
  EXPECT_EQ(300, readable[2]);
}
}  // namespace
}  // namespace test
}  // namespace webrtc
