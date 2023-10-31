/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CAPTURE_TO_SEND_ANNEX_H_
#define VIDEO_CAPTURE_TO_SEND_ANNEX_H_

#include <memory>

#include "api/units/timestamp.h"
#include "rtc_base/voucher.h"

namespace webrtc {

class RTC_EXPORT CaptureToSendCompleteAnnex : public Voucher::Annex {
 public:
  static void AttachToCurrentVoucher(Timestamp capture_reference_time);
  explicit CaptureToSendCompleteAnnex(Timestamp capture_reference_time);
  ~CaptureToSendCompleteAnnex() override;

 private:
  const Timestamp capture_reference_time_;
};

}  // namespace webrtc

#endif  // VIDEO_CAPTURE_TO_SEND_ANNEX_H_
