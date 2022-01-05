/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_PACKET_INFOS_H_
#define API_RTP_PACKET_INFOS_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "api/rtp_packet_info.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Semi-immutable structure to hold information about packets used to assemble
// an audio or video frame. Uses internal reference counting to make it very
// cheap to copy.
//
// We should ideally just use `std::vector<RtpPacketInfo>` and have it
// `std::move()`-ed as the per-packet information is transferred from one object
// to another. But moving the info, instead of copying it, is not easily done
// for the current video code.
class RTC_EXPORT RtpPacketInfos {
 public:
  using vector_type = std::vector<RtpPacketInfo>;

  using value_type = vector_type::value_type;
  using size_type = vector_type::size_type;
  using difference_type = vector_type::difference_type;
  using const_reference = vector_type::const_reference;
  using const_pointer = vector_type::const_pointer;
  using const_iterator = vector_type::const_iterator;
  using const_reverse_iterator = vector_type::const_reverse_iterator;

  using reference = const_reference;
  using pointer = const_pointer;
  using iterator = const_iterator;
  using reverse_iterator = const_reverse_iterator;

  RtpPacketInfos();
  explicit RtpPacketInfos(const vector_type& entries);
  explicit RtpPacketInfos(vector_type&& entries);

  RtpPacketInfos(const RtpPacketInfos& other);
  RtpPacketInfos(RtpPacketInfos&& other);
  RtpPacketInfos& operator=(const RtpPacketInfos& other);
  RtpPacketInfos& operator=(RtpPacketInfos&& other);

  ~RtpPacketInfos();

  const_reference operator[](size_type pos) const { return entries()[pos]; }

  const_reference at(size_type pos) const { return entries().at(pos); }
  const_reference front() const { return entries().front(); }
  const_reference back() const { return entries().back(); }

  const_iterator begin() const { return entries().begin(); }
  const_iterator end() const { return entries().end(); }
  const_reverse_iterator rbegin() const { return entries().rbegin(); }
  const_reverse_iterator rend() const { return entries().rend(); }

  const_iterator cbegin() const { return entries().cbegin(); }
  const_iterator cend() const { return entries().cend(); }
  const_reverse_iterator crbegin() const { return entries().crbegin(); }
  const_reverse_iterator crend() const { return entries().crend(); }

  bool empty() const { return entries().empty(); }
  size_type size() const { return entries().size(); }

 private:
  class Data;

  const vector_type& entries() const;
  rtc::scoped_refptr<Data> data_;
};

}  // namespace webrtc

#endif  // API_RTP_PACKET_INFOS_H_
