/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_alr_state.h"

#include "absl/memory/memory.h"

namespace webrtc {
constexpr EventParameters RtcEventAlrState::event_params_;
constexpr FieldParameters RtcEventAlrState::in_alr_params_;

RtcEventAlrState::RtcEventAlrState(bool in_alr) : in_alr_(in_alr) {}

RtcEventAlrState::RtcEventAlrState(const RtcEventAlrState& other)
    : RtcEvent(other.timestamp_us_), in_alr_(other.in_alr_) {}

RtcEventAlrState::~RtcEventAlrState() = default;

std::unique_ptr<RtcEventAlrState> RtcEventAlrState::Copy() const {
  return absl::WrapUnique<RtcEventAlrState>(new RtcEventAlrState(*this));
}

std::string RtcEventAlrState::Encode(rtc::ArrayView<const RtcEvent*> batch) {
  EventEncoder encoder(event_params_, batch);

  // Encode fields in order of increasing field IDs.
  encoder.EncodeField(in_alr_params_,
                      ExtractRtcEventMember(batch, &RtcEventAlrState::in_alr_));
  return encoder.AsString();
}

RtcEventLogParseStatus RtcEventAlrState::Parse(
    absl::string_view s,
    bool batched,
    std::vector<LoggedAlrStateEvent>& output) {
  EventParser parser;
  auto status = parser.Initialize(s, batched);
  if (!status.ok())
    return status;

  rtc::ArrayView<LoggedAlrStateEvent> output_batch =
      ExtendLoggedBatch(output, parser.NumEventsInBatch());

  constexpr FieldParameters timestamp_params{
      "timestamp_ms", FieldParameters::kTimestampField, FieldType::kVarInt, 64};
  RtcEventLogParseStatusOr<rtc::ArrayView<uint64_t>> result =
      parser.ParseNumericField(timestamp_params);
  if (!result.ok())
    return result.status();
  PopulateRtcEventTimestamp(result.value(), &LoggedAlrStateEvent::timestamp,
                            output_batch);

  // Parse fields in order of increasing field IDs.
  result = parser.ParseNumericField(in_alr_params_);
  if (!result.ok())
    return result.status();
  PopulateRtcEventMember(result.value(), &LoggedAlrStateEvent::in_alr,
                         output_batch);

  return RtcEventLogParseStatus::Success();
}

}  // namespace webrtc
