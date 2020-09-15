/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_REENCODER_REENCODE_H_
#define RTC_TOOLS_RTC_EVENT_LOG_REENCODER_REENCODE_H_

#include <string>

#include "api/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc {

bool Reencode(
    std::string inputfile,
    std::string outputfile,
    ParsedRtcEventLog::UnconfiguredHeaderExtensions unconfigured_extensions,
    RtcEventLog::EncodingType encoding);

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_REENCODER_REENCODE_H_
