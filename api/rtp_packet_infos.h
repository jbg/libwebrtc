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
#include <vector>

#include "api/rtp_packet_info.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// Semi-immutable structure to hold information about packets used to assemble
// an audio or video frame. Uses internal reference counting to make it very
// cheap to copy.
//
// We should ideally just use |std::vector<RtpPacketInfo>| and have it
// |std::move()|-ed as the per-packet information is transferred from one object
// to another. But moving the info, instead of copying it, is not easily done
// for the current video code.
class RtpPacketInfos {
 public:
  using value_type = const rtc::scoped_refptr<RtpPacketInfo>;
  using vector_type = std::vector<rtc::scoped_refptr<RtpPacketInfo>>;

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

  RtpPacketInfos() {}
  explicit RtpPacketInfos(vector_type packet_infos)
      : data_(Data::Create(packet_infos)) {}

  RtpPacketInfos(const RtpPacketInfos& other) = default;
  RtpPacketInfos(RtpPacketInfos&& other) = default;
  RtpPacketInfos& operator=(const RtpPacketInfos& other) = default;
  RtpPacketInfos& operator=(RtpPacketInfos&& other) = default;

  const_reference operator[](size_type pos) const {
    RTC_DCHECK(data_ != nullptr);
    return data_->packet_infos()[pos];
  }

  const_reference front() const {
    RTC_DCHECK(data_ != nullptr);
    return data_->packet_infos().front();
  }

  const_reference back() const {
    RTC_DCHECK(data_ != nullptr);
    return data_->packet_infos().back();
  }

  const_iterator cbegin() const {
    if (data_ == nullptr) {
      return {};
    }
    return data_->packet_infos().cbegin();
  }

  const_iterator cend() const {
    if (data_ == nullptr) {
      return {};
    }
    return data_->packet_infos().cend();
  }

  const_reverse_iterator crbegin() const {
    if (data_ == nullptr) {
      return {};
    }
    return data_->packet_infos().crbegin();
  }

  const_reverse_iterator crend() const {
    if (data_ == nullptr) {
      return {};
    }
    return data_->packet_infos().crend();
  }

  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }
  const_reverse_iterator rbegin() const { return crbegin(); }
  const_reverse_iterator rend() const { return crend(); }

  bool empty() const {
    if (data_ == nullptr) {
      return true;
    }
    return data_->packet_infos().empty();
  }

  size_type size() const {
    if (data_ == nullptr) {
      return 0;
    }

    return data_->packet_infos().size();
  }

 private:
  class Data : public rtc::RefCountInterface {
   public:
    static rtc::scoped_refptr<Data> Create(vector_type packet_infos) {
      return new rtc::RefCountedObject<Data>(packet_infos);
    }

    const vector_type& packet_infos() const { return packet_infos_; }

   private:
    friend class rtc::RefCountedObject<Data>;

    explicit Data(vector_type packet_infos) : packet_infos_(packet_infos) {}
    ~Data() override {}

    const vector_type packet_infos_;
  };

  rtc::scoped_refptr<Data> data_;
};

}  // namespace webrtc

#endif  // API_RTP_PACKET_INFOS_H_
