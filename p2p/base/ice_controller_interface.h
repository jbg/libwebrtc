/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_INTERFACE_H_
#define P2P_BASE_ICE_CONTROLLER_INTERFACE_H_

#include <utility>
#include <vector>

#include "p2p/base/connection.h"
#include "p2p/base/ice_transport_internal.h"

namespace cricket {

struct IceFieldTrials;  // Forward declaration to avoid circular dependency.

class IceControllerInterface {
 public:
  virtual ~IceControllerInterface() {}

  /**
   * These setters are called when the state of P2PTransportChannel is mutade.
   */
  virtual void SetIceConfig(const IceConfig& config) = 0;
  virtual void SetSelectedConnection(const Connection* selected_connection) = 0;
  virtual void AddConnection(const Connection* connection) = 0;
  virtual void OnConnectionDestroyed(const Connection* connection) = 0;

  // Is there a pingable connection ?
  // This function is used to boot-strap pinging, after this return true
  // SelectConnectionToPing() will be called periodically.
  virtual bool HasPingableConnection() const = 0;

  /**
   * Select a connection to Ping, or nullptr if none.
   * Also return when to call this function again as a delay in milliseconds.
   */
  virtual std::pair<Connection*, int> SelectConnectionToPing(
      int64_t last_ping_sent_ms) = 0;

  // These methods is only added to not have to change all unit tests
  // that simulate pinging by marking a connection pinged.
  virtual const Connection* FindNextPingableConnection() = 0;
  virtual void MarkConnectionPinged(const Connection* con) = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_INTERFACE_H_
