/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_PARSER_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_PARSER_H_

#include <string>
#include <vector>

#include "logging/rtc_event_log/events/rtc_event_field_encoding.h"

namespace webrtc_event_logging {

// TODO(terelius): Add context (which event and field failed to parse).
class ParseStatus {
 public:
  static ParseStatus Success() { return ParseStatus(); }
  static ParseStatus Error(std::string error, std::string file, int line) {
    return ParseStatus(error, file, line);
  }

  bool ok() const { return error_.empty(); }
  std::string message() const { return error_; }

 private:
  ParseStatus() : error_() {}
  ParseStatus(std::string error, std::string file, int line)
      : error_(error + " (" + file + ": " + std::to_string(line) + ")") {}

  std::string error_;
};

}  // namespace webrtc_event_logging

namespace webrtc {

class EventParser {
 public:
  EventParser() = default;

  // N.B: This method stores a abls::string_view into the string to be
  // parsed. The caller is responsible for ensuring that the actual string
  // remains unmodified and outlives the EventParser.
  webrtc_event_logging::ParseStatus Initialize(absl::string_view s,
                                               bool batched);

  // Attempts to parse the field specified by `params`, skipping past
  // other fields that may occur before it. Returns ParseStatus::Success
  // and populates `values` (and `positions`) if the field is found.
  // Returns ParseStatus::Success and clears `values` (and `positions`)
  // if the field doesn't exist. Returns a ParseStatus::Error
  // if the log is incomplete, malformed or otherwise can't be parsed.
  // `values` and `positions` are pure out-parameters that allow
  // the caller to reuse the same temporary storage for all fields.
  // Any previous content is cleared in ParseField. Example usage:
  // ```
  // std::vector<uint64_t> values;
  // auto status = parser.ParseField(foo_params, &values);
  // if (!status.ok()) handle_error();
  // PopulateRtcEventMember(values, &Event::foo, parsed_events);
  // status = parser.ParseField(bar_params, &values);
  // if (!status.ok()) handle_error();
  // PopulateRtcEventMember(values, &Event::bar, parsed_events);
  // ```
  webrtc_event_logging::ParseStatus ParseField(
      const FieldParameters& params,
      std::vector<uint64_t>* values,
      std::vector<bool>* positions = nullptr);

  // Number of events in a batch.
  uint64_t NumEventsInBatch() const { return num_events_; }

  // Bytes remaining in `s_`. Assuming there are no unknown
  // fields, BytesRemaining() should return 0 when all known fields
  // in the event have been parsed.
  size_t RemainingBytes() const { return s_.size(); }

 private:
  uint64_t ReadLittleEndian(uint8_t bytes);
  uint64_t ReadVarInt();
  uint64_t ReadSingleValue(FieldType field_type);
  uint64_t ReadOptionalValuePositions(std::vector<bool>* positions);
  uint64_t CountAndIgnoreOptionalValuePositions();
  void ReadDeltasAndPopulateValues(FixedLengthEncodingParametersV3 params,
                                   uint64_t num_deltas,
                                   const uint64_t base,
                                   std::vector<uint64_t>* values);

  void SetError() { error_ = true; }
  bool Ok() const { return !error_; }

  // String to be consumed.
  absl::string_view s_;

  // Tracks whgether an error has occurred in one of the helper
  // functions above.
  bool error_ = false;

  uint64_t num_events_ = 1;
  uint64_t last_field_id_ = FieldParameters::kTimestampField;
};

}  // namespace webrtc
#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_PARSER_H_
