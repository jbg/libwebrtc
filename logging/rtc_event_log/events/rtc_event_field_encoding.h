/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/events/fixed_length_encoding_parameters_v3.h"
#include "logging/rtc_event_log/events/rtc_event_field_extraction.h"
#include "rtc_base/logging.h"

namespace webrtc_event_logging {
std::string SerializeLittleEndian(uint64_t value, uint8_t bytes);

class ParseStatus {
 public:
  static ParseStatus Success() { return ParseStatus(); }
  static ParseStatus Error(std::string error, std::string file, int line) {
    return ParseStatus(error, file, line);
  }

  bool ok() const { return error_.empty() && file_.empty() && line_ == 0; }
  std::string message() const {
    return error_ + " failed at " + file_ + " line " + std::to_string(line_);
  }

  ABSL_DEPRECATED("") operator bool() const { return ok(); }

 private:
  ParseStatus() : error_(), file_(), line_(0) {}
  ParseStatus(std::string error, std::string file, int line)
      : error_(error), file_(file), line_(line) {}
  std::string error_;
  std::string file_;
  int line_;
};

}  // namespace webrtc_event_logging

namespace webrtc {

// To maintain backwards compatibility with past (or future) logs,
// the constants in this enum must not be reordered or changed.
// New field types with numerical IDs 5-7 can be added, but old
// parsers will fail to parse events containing the new fields.
enum class FieldType : uint8_t {
  kFixed8 = 0,
  kFixed32 = 1,
  kFixed64 = 2,
  kVarInt = 3,
  kString = 4,
};

struct EventParameters {
  const char* const name;
  const RtcEvent::Type id;
};

struct FieldParameters {
  const char* const name;
  const uint64_t field_id;
  const FieldType field_type;
  const uint64_t value_width;
  static constexpr uint64_t kTimestampField = 0;
};

class EventEncoder {
 public:
  EventEncoder(EventParameters params, rtc::ArrayView<const RtcEvent*> batch);

  void EncodeField(const FieldParameters& params,
                   const std::vector<uint64_t>& values,
                   const std::vector<bool>* positions = nullptr);

  void EncodeField(const FieldParameters& params,
                   const ValuesWithPositions& values);

  std::string AsString();

 private:
  size_t batch_size_;
  uint32_t event_tag_;
  std::vector<std::string> encoded_fields_;
};

class EventParser {
 public:
  EventParser() = default;

  // N.B: This method stores a abls::string_view into the string to be
  // parsed. The caller is responsible for ensuring that the actual string
  // remains unmodified and outlives the EventParser.
  webrtc_event_logging::ParseStatus Initialize(absl::string_view s,
                                               bool batched);

  webrtc_event_logging::ParseStatus ParseField(const FieldParameters& params,
                                               std::vector<uint64_t>* values);

  webrtc_event_logging::ParseStatus ParseField(const FieldParameters& params,
                                               std::vector<bool>* positions,
                                               std::vector<uint64_t>* values);

  uint64_t num_events() const { return num_events_; }
  size_t remaining_bytes() const { return s_.size(); }

 private:
  absl::string_view s_;
  bool batched_;
  uint64_t num_events_ = 1;
  uint64_t last_field_id_ = FieldParameters::kTimestampField;
};

std::string EncodeSingleValue(uint64_t value, FieldType field_type);
std::string EncodeDeltasV3(FixedLengthEncodingParametersV3 params,
                           uint64_t base,
                           rtc::ArrayView<const uint64_t> values);

}  // namespace webrtc
#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_
