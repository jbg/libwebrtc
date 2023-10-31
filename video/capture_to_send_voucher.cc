/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/capture_to_send_voucher.h"

#include <memory>

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

// Static.
void CaptureToSendCompleteAnnex::AttachToCurrentVoucher(
    Timestamp capture_reference_time) {
  static const TaskQueueBase::Voucher::Annex::Id kCaptureToEncodeAnnexId =
      TaskQueueBase::Voucher::Annex::GetNextId();
  auto voucher = TaskQueueBase::Voucher::CurrentOrCreateForCurrentTask();
  voucher->SetAnnex(
      kCaptureToEncodeAnnexId,
      std::make_unique<CaptureToSendCompleteAnnex>(capture_reference_time));
}

CaptureToSendCompleteAnnex::CaptureToSendCompleteAnnex(
    Timestamp capture_reference_time)
    : capture_reference_time_(capture_reference_time) {}

CaptureToSendCompleteAnnex::~CaptureToSendCompleteAnnex() {
  auto duration =
      Clock::GetRealTimeClock()->CurrentTime() - capture_reference_time_;
  TRACE_EVENT1("webrtc", "CaptureToSendCompleteAnnex", "duration",
               duration.us());
  RTC_HISTOGRAM_COUNTS_1000("WebRTC.Video.CaptureToSendTimeMs", duration.ms());
}

}  // namespace webrtc
