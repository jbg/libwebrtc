/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtcstatscollectorcallbackwrapper.h"

#include <string>
#include <vector>

#include "sdk/android/generated_peerconnection_jni/jni/RTCStatsCollectorCallback_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCStatsReport_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCStats_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {
namespace jni {

namespace {

template <typename T>
jobject StatsMemberToJava(JNIEnv* env, const RTCStatsMemberInterface& member) {
  return JavaFromNative(env, *member.cast_to<RTCStatsMember<T>>());
}

typedef jobject (*MemberConvertFunction)(JNIEnv* jni,
                                         const RTCStatsMemberInterface& member);

MemberConvertFunction GetConvertFunction(
    const RTCStatsMemberInterface& member) {
  switch (member.type()) {
    case RTCStatsMemberInterface::kBool:
      return &StatsMemberToJava<bool>;

    case RTCStatsMemberInterface::kInt32:
      return &StatsMemberToJava<int32_t>;

    case RTCStatsMemberInterface::kUint32:
      return &StatsMemberToJava<uint32_t>;

    case RTCStatsMemberInterface::kInt64:
      return &StatsMemberToJava<int64_t>;

    case RTCStatsMemberInterface::kUint64:
      return &StatsMemberToJava<uint64_t>;

    case RTCStatsMemberInterface::kDouble:
      return &StatsMemberToJava<double>;

    case RTCStatsMemberInterface::kString:
      return &StatsMemberToJava<std::string>;

    case RTCStatsMemberInterface::kSequenceBool:
      return &StatsMemberToJava<std::vector<bool>>;

    case RTCStatsMemberInterface::kSequenceInt32:
      return &StatsMemberToJava<std::vector<int32_t>>;

    case RTCStatsMemberInterface::kSequenceUint32:
      return &StatsMemberToJava<std::vector<uint32_t>>;

    case RTCStatsMemberInterface::kSequenceInt64:
      return &StatsMemberToJava<std::vector<int64_t>>;

    case RTCStatsMemberInterface::kSequenceUint64:
      return &StatsMemberToJava<std::vector<uint64_t>>;

    case RTCStatsMemberInterface::kSequenceDouble:
      return &StatsMemberToJava<std::vector<double>>;

    case RTCStatsMemberInterface::kSequenceString:
      return &StatsMemberToJava<std::vector<std::string>>;
  }
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace

template <>
jobject JavaFromNative(JNIEnv* env, const RTCStatsMemberInterface& member) {
  return GetConvertFunction(member)(env, member);
}

RTCStatsCollectorCallbackWrapper::RTCStatsCollectorCallbackWrapper(
    JNIEnv* jni,
    jobject j_callback)
    : j_callback_global_(jni, j_callback),
      j_linked_hash_map_class_(FindClass(jni, "java/util/LinkedHashMap")),
      j_linked_hash_map_ctor_(
          GetMethodID(jni, j_linked_hash_map_class_, "<init>", "()V")),
      j_linked_hash_map_put_(GetMethodID(
          jni,
          j_linked_hash_map_class_,
          "put",
          "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")) {}

void RTCStatsCollectorCallbackWrapper::OnStatsDelivered(
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_report = ReportToJava(jni, report);
  Java_RTCStatsCollectorCallback_onStatsDelivered(jni, *j_callback_global_,
                                                  j_report);
}

jobject RTCStatsCollectorCallbackWrapper::ReportToJava(
    JNIEnv* jni,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  jobject j_stats_map =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  for (const RTCStats& stats : *report) {
    // Create a local reference frame for each RTCStats, since there is a
    // maximum number of references that can be created in one frame.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_id = JavaStringFromStdString(jni, stats.id());
    jobject j_stats = StatsToJava(jni, stats);
    jni->CallObjectMethod(j_stats_map, j_linked_hash_map_put_, j_id, j_stats);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_report =
      Java_RTCStatsReport_create(jni, report->timestamp_us(), j_stats_map);
  return j_report;
}

jobject RTCStatsCollectorCallbackWrapper::StatsToJava(JNIEnv* jni,
                                                      const RTCStats& stats) {
  jstring j_type = JavaStringFromStdString(jni, stats.type());
  jstring j_id = JavaStringFromStdString(jni, stats.id());
  jobject j_members =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  for (const RTCStatsMemberInterface* member : stats.Members()) {
    if (!member->is_defined()) {
      continue;
    }
    // Create a local reference frame for each member as well.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_name = JavaStringFromStdString(jni, member->name());
    jobject j_member = JavaFromNative(jni, *member);
    jni->CallObjectMethod(j_members, j_linked_hash_map_put_, j_name, j_member);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_stats =
      Java_RTCStats_create(jni, stats.timestamp_us(), j_type, j_id, j_members);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_stats;
}

}  // namespace jni
}  // namespace webrtc
