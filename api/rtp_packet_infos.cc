/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_infos.h"

#include "api/ref_counted_base.h"

namespace webrtc {

namespace {
const RtpPacketInfos::vector_type& EmptyEntries() {
  static const RtpPacketInfos::vector_type& value =
      *new RtpPacketInfos::vector_type();
  return value;
}

}  // namespace

class RtpPacketInfos::Data : public rtc::RefCountedBase {
 public:
  static rtc::scoped_refptr<Data> Create(const vector_type& entries) {
    // Performance optimization for the empty case.
    if (entries.empty()) {
      return nullptr;
    }

    return new Data(entries);
  }

  static rtc::scoped_refptr<Data> Create(vector_type&& entries) {
    // Performance optimization for the empty case.
    if (entries.empty()) {
      return nullptr;
    }

    return new Data(std::move(entries));
  }

  const vector_type& entries() const { return entries_; }

 private:
  explicit Data(const vector_type& entries) : entries_(entries) {}
  explicit Data(vector_type&& entries) : entries_(std::move(entries)) {}
  ~Data() override {}

  const vector_type entries_;
};

RtpPacketInfos::RtpPacketInfos() = default;
RtpPacketInfos::RtpPacketInfos(const vector_type& entries)
    : data_(Data::Create(entries)) {}

RtpPacketInfos::RtpPacketInfos(vector_type&& entries)
    : data_(Data::Create(std::move(entries))) {}

RtpPacketInfos::RtpPacketInfos(const RtpPacketInfos& other) = default;
RtpPacketInfos::RtpPacketInfos(RtpPacketInfos&& other) = default;

RtpPacketInfos& RtpPacketInfos::operator=(const RtpPacketInfos& other) =
    default;
RtpPacketInfos& RtpPacketInfos::operator=(RtpPacketInfos&& other) = default;

RtpPacketInfos::~RtpPacketInfos() = default;

const RtpPacketInfos::vector_type& RtpPacketInfos::entries() const {
  if (data_ != nullptr) {
    return data_->entries();
  } else {
    return EmptyEntries();
  }
}

}  // namespace webrtc
