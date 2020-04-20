/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_fec_generator.h"

#include <utility>

#include "rtc_base/critical_section.h"

namespace webrtc {

VideoFecGenerator::VideoFecGenerator() = default;
VideoFecGenerator::~VideoFecGenerator() = default;

class SynchronizedVideoFecGeneratorWrapper : public VideoFecGenerator {
 public:
  explicit SynchronizedVideoFecGeneratorWrapper(
      std::unique_ptr<VideoFecGenerator> impl)
      : impl_(std::move(impl)) {}
  virtual ~SynchronizedVideoFecGeneratorWrapper() = default;

  FecType GetFecType() const override {
    rtc::CritScope cs(&lock_);
    return impl_->GetFecType();
  }

  absl::optional<uint32_t> FecSsrc() override {
    rtc::CritScope cs(&lock_);
    return impl_->FecSsrc();
  }

  size_t MaxPacketOverhead() const override {
    rtc::CritScope cs(&lock_);
    return impl_->MaxPacketOverhead();
  }

  DataRate CurrentFecRate() const override {
    rtc::CritScope cs(&lock_);
    return impl_->CurrentFecRate();
  }

  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override {
    rtc::CritScope cs(&lock_);
    impl_->SetProtectionParameters(delta_params, key_params);
  }

  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override {
    rtc::CritScope cs(&lock_);
    impl_->AddPacketAndGenerateFec(packet);
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override {
    rtc::CritScope cs(&lock_);
    return impl_->GetFecPackets();
  }

  rtc::CriticalSection lock_;
  std::unique_ptr<VideoFecGenerator> const impl_ RTC_GUARDED_BY(&lock_);
};

std::unique_ptr<VideoFecGenerator> VideoFecGenerator::MakeSynchronized(
    std::unique_ptr<VideoFecGenerator> fec_generator) {
  return std::make_unique<SynchronizedVideoFecGeneratorWrapper>(
      std::move(fec_generator));
}

}  // namespace webrtc
