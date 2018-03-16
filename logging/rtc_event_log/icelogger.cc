/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/icelogger.h"

#include <utility>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"

namespace {

using webrtc::IceEvent;
using webrtc::CandidatePairEvent;
using webrtc::CandidatePairConfigEvent;
using webrtc::ConnectivityCheckEvent;

using webrtc::IceCandidatePairEventType;
using webrtc::IceCandidatePairDescription;

IceCandidatePairEventType ConvertToRtcEventLogType(
    CandidatePairConfigEvent::Type type) {
  switch (type) {
    case CandidatePairConfigEvent::Type::DESTROYED:
      return IceCandidatePairEventType::kDestroyed;
    case CandidatePairConfigEvent::Type::ADDED:
      return IceCandidatePairEventType::kAdded;
    case CandidatePairConfigEvent::Type::UPDATED:
      return IceCandidatePairEventType::kUpdated;
    case CandidatePairConfigEvent::Type::SELECTED:
      return IceCandidatePairEventType::kSelected;
    default:
      RTC_NOTREACHED();
  }
  return IceCandidatePairEventType::kDestroyed;
}

webrtc::IceCandidatePairEventType ConvertToRtcEventLogType(
    ConnectivityCheckEvent::Type type) {
  switch (type) {
    case ConnectivityCheckEvent::Type::CHECK_SENT:
      return IceCandidatePairEventType::kCheckSent;
    case ConnectivityCheckEvent::Type::CHECK_RECEIVED:
      return IceCandidatePairEventType::kCheckReceived;
    case ConnectivityCheckEvent::Type::CHECK_RESPONSE_SENT:
      return IceCandidatePairEventType::kCheckResponseSent;
    case ConnectivityCheckEvent::Type::CHECK_RESPONSE_RECEIVED:
      return IceCandidatePairEventType::kCheckResponseReceived;
    default:
      RTC_NOTREACHED();
  }
  return IceCandidatePairEventType::kCheckSent;
}

enum class IceEventTerminalType {
  //
  NON_TERMINAL,
  // Candidate pair config events.
  DESTROYED,
  ADDED,
  UPDATED,
  SELECTED,
  // Connectivity check events.
  CHECK_SENT,
  CHECK_RECEIVED,
  CHECK_RESPONSE_SENT,
  CHECK_RESPONSE_RECEIVED,
};

// Helper methods to create imcomplete ICE events at differents levels in the
// ICE event hierarchy, and they can be chained to create a complete ICE event
// at the terminal level. The convention is to provide the pointer to the
// parent-level event and to return the pointer to the event at the current
// level where the creation is invoked, except for the root level. The
// arguments in each method signature are the fields required at the current
// level except the type field, which is left to be filled by the next level
// or manually.

void CreateIceEventInternal(IceEvent* root_event, int64_t timestamp) {
  root_event->timestamp = timestamp;
}

CandidatePairEvent* CreateCandidatePairEventInternal(
    IceEvent* parent_event,
    uint32_t candidate_pair_id,
    const IceCandidatePairDescription& candidate_pair_description) {
  parent_event->type = IceEvent::Type::CANDIDATE_PAIR_EVENT;
  CandidatePairEvent candidate_pair_event;
  candidate_pair_event.candidate_pair_id = candidate_pair_id;
  candidate_pair_event.candidate_pair_description = candidate_pair_description;
  parent_event->candidate_pair_event = candidate_pair_event;
  return &parent_event->candidate_pair_event.value();
}

CandidatePairConfigEvent* CreateCandidatePairConfigEventInternal(
    CandidatePairEvent* parent_event) {
  parent_event->type = CandidatePairEvent::Type::CONFIG;
  CandidatePairConfigEvent candidate_pair_config_event;
  parent_event->candidate_pair_config_event = candidate_pair_config_event;
  return &parent_event->candidate_pair_config_event.value();
}
ConnectivityCheckEvent* CreateConnectivityCheckEventInternal(
    CandidatePairEvent* parent_event) {
  parent_event->type = CandidatePairEvent::Type::CONNECTIVITY_CHECK;
  ConnectivityCheckEvent connectivity_check_event;
  parent_event->connectivity_check_event = connectivity_check_event;
  return &parent_event->connectivity_check_event.value();
}

}  // namespace

namespace webrtc {

CandidatePairConfigEvent::CandidatePairConfigEvent() = default;
CandidatePairConfigEvent::~CandidatePairConfigEvent() = default;
int CandidatePairConfigEvent::terminal_event_type_hash() {
  switch (type) {
    case CandidatePairConfigEvent::Type::DESTROYED:
      return static_cast<int>(IceEventTerminalType::DESTROYED);
    case CandidatePairConfigEvent::Type::ADDED:
      return static_cast<int>(IceEventTerminalType::ADDED);
    case CandidatePairConfigEvent::Type::UPDATED:
      return static_cast<int>(IceEventTerminalType::UPDATED);
    case CandidatePairConfigEvent::Type::SELECTED:
      return static_cast<int>(IceEventTerminalType::SELECTED);
    default:
      RTC_NOTREACHED();
  }
}

ConnectivityCheckEvent::ConnectivityCheckEvent() = default;
ConnectivityCheckEvent::~ConnectivityCheckEvent() = default;
int ConnectivityCheckEvent::terminal_event_type_hash() {
  switch (type) {
    case ConnectivityCheckEvent::Type::CHECK_SENT:
      return static_cast<int>(IceEventTerminalType::CHECK_SENT);
    case ConnectivityCheckEvent::Type::CHECK_RECEIVED:
      return static_cast<int>(IceEventTerminalType::CHECK_RECEIVED);
    case ConnectivityCheckEvent::Type::CHECK_RESPONSE_SENT:
      return static_cast<int>(IceEventTerminalType::CHECK_RESPONSE_SENT);
    case ConnectivityCheckEvent::Type::CHECK_RESPONSE_RECEIVED:
      return static_cast<int>(IceEventTerminalType::CHECK_RESPONSE_RECEIVED);
    default:
      RTC_NOTREACHED();
  }
}

CandidatePairEvent::CandidatePairEvent() = default;
CandidatePairEvent::~CandidatePairEvent() = default;

IceEvent::IceEvent()
    : terminal_event_type_hash(
          static_cast<int>(IceEventTerminalType::NON_TERMINAL)) {}
IceEvent::~IceEvent() = default;

IceEventStats::IceEventStats() = default;

IceEventLog::IceEventLog() {}
IceEventLog::~IceEventLog() {}

std::unique_ptr<IceEvent> IceEventLog::CreateCandidatePairConfigEvent(
    uint32_t timestamp,
    uint32_t candidate_pair_id,
    const IceCandidatePairDescription& candidate_pair_description,
    CandidatePairConfigEvent::Type type) {
  std::unique_ptr<IceEvent> root_event(new IceEvent());
  CreateIceEventInternal(root_event.get(), timestamp);
  auto* candidate_pair_event = CreateCandidatePairEventInternal(
      root_event.get(), candidate_pair_id, candidate_pair_description);
  auto* candidate_pair_config_event =
      CreateCandidatePairConfigEventInternal(candidate_pair_event);
  candidate_pair_config_event->type = type;
  return root_event;
}

std::unique_ptr<IceEvent> IceEventLog::CreateConnectivityCheckEvent(
    uint32_t timestamp,
    uint32_t candidate_pair_id,
    const IceCandidatePairDescription& candidate_pair_description,
    ConnectivityCheckEvent::Type type,
    rtc::Optional<int> rtt) {
  std::unique_ptr<IceEvent> root_event(new IceEvent());
  CreateIceEventInternal(root_event.get(), timestamp);
  auto* candidate_pair_event = CreateCandidatePairEventInternal(
      root_event.get(), candidate_pair_id, candidate_pair_description);
  auto* connectivity_check_event =
      CreateConnectivityCheckEventInternal(candidate_pair_event);
  connectivity_check_event->type = type;
  connectivity_check_event->rtt = rtt;
  return root_event;
}

void IceEventLog::DumpCandidatePairDescriptionToRtcEventLog() const {
  for (const auto& desc_id_pair : candidate_pair_desc_by_id_) {
    event_log_->Log(rtc::MakeUnique<RtcEventIceCandidatePairConfig>(
        IceCandidatePairEventType::kUpdated, desc_id_pair.first,
        desc_id_pair.second));
  }
}

void IceEventLog::LogIceEvent(std::unique_ptr<IceEvent> event) {
  const IceEvent& event_ref = *event;
  switch (event->type) {
    case IceEvent::Type::CANDIDATE_PAIR_EVENT: {
      RTC_DCHECK(event->candidate_pair_event);
      LogCandidatePairEvent(std::move(event));
      break;
    }
    default:
      RTC_NOTREACHED();
  }
  UpdateStats(event_ref);
}

void IceEventLog::UpdateStats(const IceEvent& event) {
  switch (static_cast<IceEventTerminalType>(event.terminal_event_type_hash)) {
    case IceEventTerminalType::SELECTED: {
      stats_.total_network_switching++;
      IceCandidateNetworkType new_active_network =
          event.candidate_pair_event.value()
              .candidate_pair_description.local_network_type;
      if (new_active_network == IceCandidateNetworkType::kWifi) {
        stats_.total_candidate_pairs_added_on_wifi++;
      }
      if (new_active_network == IceCandidateNetworkType::kCellular) {
        stats_.total_candidate_pairs_added_on_cell++;
      }
      if (active_network_ == IceCandidateNetworkType::kWifi &&
          new_active_network == IceCandidateNetworkType::kCellular) {
        stats_.total_network_switching_wifi_to_cell++;
      }
      if (active_network_ == IceCandidateNetworkType::kCellular &&
          new_active_network == IceCandidateNetworkType::kWifi) {
        stats_.total_network_switching_cell_to_wifi++;
      }
      break;
    }
    case IceEventTerminalType::CHECK_SENT: {
      stats_.total_checks_sent++;
      break;
    }
    case IceEventTerminalType::CHECK_RECEIVED: {
      stats_.total_checks_received++;
      break;
    }
    case IceEventTerminalType::CHECK_RESPONSE_SENT: {
      stats_.total_check_responses_sent++;
      break;
    }
    case IceEventTerminalType::CHECK_RESPONSE_RECEIVED: {
      stats_.total_check_responses_received++;
      auto rtt = event.candidate_pair_event.value()
                     .connectivity_check_event.value()
                     .rtt.value_or(-1);
      RTC_DCHECK(rtt > 0);
      stats_.total_check_rtt_ms += rtt;
      stats_.total_check_rtt_ms_squared += rtt * rtt;
      break;
    }
    default:
      return;
  }
}

void IceEventLog::LogCandidatePairEvent(std::unique_ptr<IceEvent> event) {
  const CandidatePairEvent& candidate_pair_event =
      event->candidate_pair_event.value();
  switch (candidate_pair_event.type) {
    case CandidatePairEvent::Type::CONFIG: {
      candidate_pair_config_events_.push_back(std::move(event));
      break;
    }
    case CandidatePairEvent::Type::CONNECTIVITY_CHECK: {
      connectivity_check_events_.push_back(std::move(event));
      break;
    }
    default:
      RTC_NOTREACHED();
  }
  LogCandidatePairEventToRtcEventLog(candidate_pair_event);
}

void IceEventLog::LogCandidatePairEventToRtcEventLog(
    const CandidatePairEvent& event) {
  if (event_log_ == nullptr) {
    return;
  }
  switch (event.type) {
    case CandidatePairEvent::Type::CONFIG: {
      RTC_DCHECK(event.candidate_pair_config_event);
      const CandidatePairConfigEvent::Type subtype =
          event.candidate_pair_config_event.value().type;
      candidate_pair_desc_by_id_[event.candidate_pair_id] =
          event.candidate_pair_description;
      event_log_->Log(rtc::MakeUnique<RtcEventIceCandidatePairConfig>(
          ConvertToRtcEventLogType(subtype), event.candidate_pair_id,
          event.candidate_pair_description));
      return;
    }
    case CandidatePairEvent::Type::CONNECTIVITY_CHECK: {
      RTC_DCHECK(event.connectivity_check_event);
      const ConnectivityCheckEvent::Type subtype =
          event.connectivity_check_event.value().type;
      event_log_->Log(rtc::MakeUnique<RtcEventIceCandidatePair>(
          ConvertToRtcEventLogType(subtype), event.candidate_pair_id));
      return;
    }
    default:
      RTC_NOTREACHED();
  }
}

}  // namespace webrtc
