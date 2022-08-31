/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_ACK_TYPES_H_
#define P2P_BASE_ICE_CONTROLLER_ACK_TYPES_H_

#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "p2p/base/ice_recheck_event.h"
#include "p2p/base/ice_switch_reason.h"

namespace cricket {

// An acknowledgement for a PingRequest.
struct PingAcknowledgement {
  PingAcknowledgement(uint32_t _connection_id, int _recheck_delay_ms)
      : connection_id(_connection_id), recheck_delay_ms(_recheck_delay_ms) {}

  // ID of the connection to ping.
  const uint32_t connection_id;

  // Optional delay before the next attempt to select and ping a connection.
  const int recheck_delay_ms = 0;
};

// An acknowledgement for a SwitchRequest.
struct SwitchAcknowledgement {
  SwitchAcknowledgement(
      IceSwitchReason _reason,
      uint32_t _connection_id,
      absl::optional<IceRecheckEvent> _recheck_event,
      std::vector<const uint32_t> _connection_ids_to_forget_state_on,
      bool _perform_prune)
      : reason(_reason),
        connection_id(_connection_id),
        recheck_event(_recheck_event),
        connection_ids_to_forget_state_on(
            std::move(_connection_ids_to_forget_state_on)),
        perform_prune(_perform_prune) {}

  // Reason for which the requested switch was initiated.
  const IceSwitchReason reason;

  // ID of the connection to switch to.
  const uint32_t connection_id;

  // An optional event describing the next switch recheck.
  const absl::optional<IceRecheckEvent> recheck_event;

  // A vector of IDs for connections to forget learned state for.
  const std::vector<const uint32_t> connection_ids_to_forget_state_on;

  // Whether a prune should be performed after the switch.
  const bool perform_prune;
};

// An acknowledgement for a PruneRequest.
struct PruneAcknowledgement {
  explicit PruneAcknowledgement(
      std::vector<const uint32_t> _connection_ids_to_prune,
      bool _only_handle_resort)
      : connection_ids_to_prune(std::move(_connection_ids_to_prune)),
        only_handle_resort(_only_handle_resort) {}

  // A vector of IDs for connections to prune.
  const std::vector<const uint32_t> connection_ids_to_prune;

  // Set if the prune request was rejected, in which case we still need to
  // handle the resorting. This could be indicated by just setting the
  // connections to an empty list, but stated explicitly for clarity.
  const bool only_handle_resort;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_ACK_TYPES_H_
