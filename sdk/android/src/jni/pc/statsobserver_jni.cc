/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/statsobserver_jni.h"

#include "sdk/android/generated_peerconnection_jni/jni/StatsObserver_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/StatsReport_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

namespace {

jobjectArray JavaArrayFromNativeMap(JNIEnv* env,
                                    const StatsReport::Values& value_map) {
  // Ignore the keys and make an array out of the values.
  std::vector<StatsReport::ValuePtr> values;
  for (const auto& it : value_map)
    values.push_back(it.second);
  return JavaArrayFromNative(env, values);
}

}  // namespace

StatsObserverJni::StatsObserverJni(JNIEnv* jni, jobject j_observer)
    : j_observer_global_(jni, j_observer) {}

template <>
jclass GetCorrespondingJavaClass<StatsReports::value_type>(JNIEnv* env) {
  return org_webrtc_StatsReport_clazz(env);
}

template <>
jclass GetCorrespondingJavaClass<StatsReport::ValuePtr>(JNIEnv* env) {
  return org_webrtc_StatsReport_00024Value_clazz(env);
}

void StatsObserverJni::OnComplete(const StatsReports& reports) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_StatsObserver_onComplete(env, *j_observer_global_,
                                JavaArrayFromNative(env, reports));
}

template <>
jobject JavaFromNative(JNIEnv* env, const StatsReports::value_type& report) {
  jstring j_id = JavaStringFromStdString(env, report->id()->ToString());
  jstring j_type = JavaStringFromStdString(env, report->TypeToString());
  jobjectArray j_values = JavaArrayFromNativeMap(env, report->values());
  return Java_StatsReport_Constructor(env, j_id, j_type, report->timestamp(),
                                      j_values);
}

template <>
jobject JavaFromNative(JNIEnv* env, const StatsReport::ValuePtr& value_ptr) {
  // Should we use the '.name' enum value here instead of converting the
  // name to a string?
  jstring j_name = JavaStringFromStdString(env, value_ptr->display_name());
  jstring j_value = JavaStringFromStdString(env, value_ptr->ToString());
  return Java_Value_Constructor(env, j_name, j_value);
}

}  // namespace jni
}  // namespace webrtc
