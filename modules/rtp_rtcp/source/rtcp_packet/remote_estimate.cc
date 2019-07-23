/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtcp_packet/remote_estimate.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <vector>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"

namespace webrtc {
namespace rtcp {
namespace {

template <typename T>
T GetMaxVal() {
  return T::PlusInfinity();
}

class RemoteEstimateSerializerImpl : public RemoteEstimateSerializer {
 public:
  static constexpr int kBytes = 3;
  static constexpr int kFieldSize = 1 + kBytes;
  struct FieldSerializer {
    uint8_t id;
    std::function<void(const uint8_t* src, NetworkStateEstimate* target)> read;
    std::function<void(const NetworkStateEstimate& src, uint8_t* target)> write;
  };
  template <typename T, typename Closure>
  static FieldSerializer Field(uint8_t id, T resolution, Closure field_getter) {
    constexpr uint32_t kMaxEncoded = (1 << (kBytes * 8)) - 1;
    return FieldSerializer{
        id,
        [=](const uint8_t* src, NetworkStateEstimate* target) {
          uint32_t scaled =
              ByteReader<uint32_t, kBytes, false>::ReadBigEndian(src);
          if (scaled == kMaxEncoded) {
            *field_getter(target) = GetMaxVal<T>();
          } else {
            *field_getter(target) = resolution * static_cast<int64_t>(scaled);
          }
        },
        [=](const NetworkStateEstimate& src, uint8_t* target) {
          auto value = *field_getter(const_cast<NetworkStateEstimate*>(&src));
          uint32_t scaled =
              value == GetMaxVal<T>()
                  ? kMaxEncoded
                  : std::max<int64_t>(
                        0, std::min<int64_t>(static_cast<int64_t>(std::round(
                                                 value / resolution)),
                                             kMaxEncoded));
          ByteWriter<uint32_t, kBytes, false>::WriteBigEndian(target, scaled);
        }};
  }

  explicit RemoteEstimateSerializerImpl(std::vector<FieldSerializer> fields)
      : fields_(fields) {}

  rtc::Buffer Serialize(const NetworkStateEstimate& src) override {
    size_t size = fields_.size() * kFieldSize;
    rtc::Buffer buf(size);
    uint8_t* target_ptr = buf.data();
    for (const auto& field : fields_) {
      ByteWriter<uint8_t>::WriteBigEndian(target_ptr++, field.id);
      field.write(src, target_ptr);
      target_ptr += kBytes;
    }
    return buf;
  }

  bool Parse(rtc::ArrayView<const uint8_t> src,
             NetworkStateEstimate* target) override {
    if (src.size() % kFieldSize != 0)
      return false;
    RTC_DCHECK_EQ(src.size() % kFieldSize, 0);
    for (const uint8_t* data_ptr = src.data(); data_ptr < src.end();
         data_ptr += kFieldSize) {
      uint8_t field_id = ByteReader<uint8_t>::ReadBigEndian(data_ptr);
      for (const auto& field : fields_) {
        if (field.id == field_id) {
          field.read(data_ptr + 1, target);
          break;
        }
      }
    }
    return true;
  }

 private:
  const std::vector<FieldSerializer> fields_;
};

}  // namespace

RemoteEstimateSerializer* GetRemoteEstimateSerializer() {
  using E = NetworkStateEstimate;
  using S = RemoteEstimateSerializerImpl;
  static RemoteEstimateSerializerImpl* serializer =
      new RemoteEstimateSerializerImpl({
          S::Field(1, DataRate::kbps(1),
                   [](E* e) { return &e->link_capacity_lower; }),
          S::Field(2, DataRate::kbps(1),
                   [](E* e) { return &e->link_capacity_upper; }),
      });
  return serializer;
}

RemoteEstimate::RemoteEstimate() : serializer_(GetRemoteEstimateSerializer()) {
  SetSubType(kSubType);
  SetName(kName);
  SetSsrc(0);
}

bool RemoteEstimate::IsNetworkEstimate(const CommonHeader& packet) {
  if (packet.fmt() != kSubType)
    return false;
  if (packet.packet_size() < 8)
    return false;
  if (ByteReader<uint32_t>::ReadBigEndian(&packet.payload()[4]) != kName)
    return false;
  return true;
}

bool RemoteEstimate::Parse(const CommonHeader& packet) {
  if (!App::Parse(packet))
    return false;
  return serializer_->Parse({data(), data_size()}, &estimate_);
}

void RemoteEstimate::SetEstimate(NetworkStateEstimate estimate) {
  estimate_ = estimate;
  auto buf = serializer_->Serialize(estimate);
  SetData(buf.data(), buf.size());
}

}  // namespace rtcp
}  // namespace webrtc
