/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/regatheringcontroller.h"

namespace webrtc {

using Config = BasicRegatheringController::Config;

Config::Config(int regather_on_failed_networks_interval,
               const rtc::Optional<rtc::IntervalRange>&
                   regather_on_all_networks_interval_range)
    : regather_on_failed_networks_interval(
          regather_on_failed_networks_interval),
      regather_on_all_networks_interval_range(
          regather_on_all_networks_interval_range) {}

Config::Config(const Config& other) = default;

Config::~Config() = default;
Config& Config::operator=(const BasicRegatheringController::Config& other) =
    default;

BasicRegatheringController::BasicRegatheringController(
    cricket::IceTransportInternal* ice_transport,
    const Config& config,
    rtc::Thread* thread)
    : ice_transport_(ice_transport),
      config_(config),
      thread_(thread),
      rand_(rtc::SystemTimeNanos()) {
  ice_transport_->SignalStateChanged.connect(
      this, &BasicRegatheringController::OnIceTransportStateChanged);
  ice_transport->SignalWritableState.connect(
      this, &BasicRegatheringController::OnIceTransportWritableState);
  ice_transport->SignalReceivingState.connect(
      this, &BasicRegatheringController::OnIceTransportReceivingState);
  ice_transport->SignalNetworkRouteChanged.connect(
      this, &BasicRegatheringController::OnIceTransportNetworkRouteChanged);
}

BasicRegatheringController::~BasicRegatheringController() = default;

void BasicRegatheringController::ScheduleRegatheringOnAllNetworks(
    rtc::IntervalRange delay_ms_range,
    bool repeated) {
  int delay_ms = SampleRegatherAllNetworksInterval(delay_ms_range);
  rtc::Optional<rtc::IntervalRange> next_schedule_delay_ms_range(rtc::nullopt);
  if (repeated) {
    CancelScheduledRegathering();
    next_schedule_delay_ms_range = delay_ms_range;
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering,
          this, repeated, next_schedule_delay_ms_range),
      delay_ms);
}

void BasicRegatheringController::ScheduleRegatheringOnAllNetworks(
    bool repeated) {
  RTC_DCHECK(config_.regather_on_all_networks_interval_range);
  if (!config_.regather_on_all_networks_interval_range) {
    RTC_LOG(LS_ERROR)
        << "No default interval configured for regathering on all networks";
    return;
  }
  ScheduleRegatheringOnAllNetworks(
      config_.regather_on_all_networks_interval_range.value(), repeated);
}

void BasicRegatheringController::RegatherOnAllNetworksIfDoneGathering(
    bool repeated,
    rtc::Optional<rtc::IntervalRange> next_schedule_delay_ms_range) {
  // Only re-gather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    allocator_session_->RegatherOnAllNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms_range);
    ScheduleRegatheringOnAllNetworks(next_schedule_delay_ms_range.value(),
                                     true);
  }
}

void BasicRegatheringController::ScheduleRegatheringOnFailedNetworks(
    int delay_ms,
    bool repeated) {
  rtc::Optional<int> next_schedule_delay_ms(rtc::nullopt);
  if (repeated) {
    CancelScheduledRegathering();
    next_schedule_delay_ms = delay_ms;
  }
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(
          &BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering,
          this, repeated, next_schedule_delay_ms),
      delay_ms);
}

void BasicRegatheringController::ScheduleRegatheringOnFailedNetworks(
    bool repeated) {
  ScheduleRegatheringOnFailedNetworks(
      config_.regather_on_failed_networks_interval, repeated);
}

void BasicRegatheringController::RegatherOnFailedNetworksIfDoneGathering(
    bool repeated,
    rtc::Optional<int> next_schedule_delay_ms) {
  // Only regather when the current session is in the CLEARED state (i.e., not
  // running or stopped). It is only possible to enter this state when we gather
  // continually, so there is an implicit check on continual gathering here.
  if (allocator_session_ && allocator_session_->IsCleared()) {
    allocator_session_->RegatherOnFailedNetworks();
  }
  if (repeated) {
    RTC_DCHECK(next_schedule_delay_ms);
    ScheduleRegatheringOnFailedNetworks(next_schedule_delay_ms.value(), true);
  }
}

void BasicRegatheringController::CancelScheduledRegathering() {
  invoker_.Clear();
}

int BasicRegatheringController::SampleRegatherAllNetworksInterval(
    const rtc::IntervalRange& range) {
  return rand_.Rand(range.min(), range.max());
}

}  // namespace webrtc
