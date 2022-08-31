/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_
#define P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_

#include <vector>

#include "absl/types/optional.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_recheck_event.h"
#include "p2p/base/ice_switch_reason.h"

namespace cricket {

// TODO(samvi) Migrate cricket::IceControllerInterface::PingResult to this.
// This represents the result of a call to SelectConnectionToPing.
struct PingRequest {
  PingRequest(absl::optional<const Connection*> _connection,
              int _recheck_delay_ms)
      : connection(_connection),
        recheck_delay_ms(_recheck_delay_ms > 0
                             ? absl::make_optional<int>(_recheck_delay_ms)
                             : absl::nullopt) {}

  // Connection that we should (optionally) ping.
  const absl::optional<const Connection*> connection;

  // The delay before another connection is selected to Ping.
  const absl::optional<int> recheck_delay_ms;
};

// TODO(samvi) Migrate cricket::IceControllerInterface::SwitchResult to this.
// This represents the result of a switch call.
struct SwitchRequest {
  SwitchRequest(IceSwitchReason _reason,
                absl::optional<const Connection*> _connection,
                absl::optional<IceRecheckEvent> _recheck_event,
                std::vector<const Connection*> _connections_to_forget_state_on,
                bool _cancelable,
                bool _requires_pruning)
      : reason(_reason),
        connection(_connection),
        recheck_event(_recheck_event),
        connections_to_forget_state_on(_connections_to_forget_state_on),
        cancelable(_cancelable),
        requires_pruning(_requires_pruning) {}

  // The reason for which this switch was initiated.
  IceSwitchReason reason;

  // Connection that we should (optionally) switch to.
  absl::optional<const Connection*> connection;

  // An optional recheck event for when a Switch() should be attempted again.
  absl::optional<IceRecheckEvent> recheck_event;

  // A vector with connection to run ForgetLearnedState on.
  std::vector<const Connection*> connections_to_forget_state_on;

  // The below booleans should be derived from the IceSwitchReason, but
  // explicitly stated here for now for simplicity.

  // Whether the request can be canceled. A switch may not be canceled if, for
  // instance, it is at the controlled agent because of an indication from the
  // controlling agent.
  bool cancelable;

  // Whether a prune must be performed after the switch. Pruning shouldn't be
  // done after certain switches, such as when a switch is initiated on the
  // controlled side due to a renomination from the controlling side.
  bool requires_pruning;
};

// This represents the result of a call to SelectConnectionsToPrune.
struct PruneRequest {
  explicit PruneRequest(std::vector<const Connection*> _connections_to_prune)
      : connections_to_prune(_connections_to_prune) {}

  // A vector with connections to prune.
  std::vector<const Connection*> connections_to_prune;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_
