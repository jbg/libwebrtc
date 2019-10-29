/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_BASIC_ICE_CONTROLLER_H_
#define P2P_BASE_BASIC_ICE_CONTROLLER_H_

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/p2p_transport_channel.h"

namespace cricket {

class BasicIceController : public IceControllerInterface {
 public:
  BasicIceController(
      std::function<IceTransportState()> ice_transport_state_func,
      std::function<rtc::ArrayView<const Connection*>()>
          sorted_connection_list_func,
      const IceFieldTrials*);
  virtual ~BasicIceController();

  void SetIceConfig(const IceConfig& config) override;
  void SetSelectedConnection(const Connection* selected_connection) override;
  void AddConnection(const Connection* connection) override;
  void OnConnectionDestroyed(const Connection* connection) override;

  bool HasPingableConnection() const override;

  std::pair<Connection*, int> SelectConnectionToPing(
      int64_t last_ping_sent_ms) override;

  // These methods is only for tests.
  const Connection* FindNextPingableConnection() override;
  void MarkConnectionPinged(const Connection* conn) override;

 private:
  // A transport channel is weak if the current best connection is either
  // not receiving or not writable, or if there is no best connection at all.
  bool weak() const {
    return !selected_connection_ || selected_connection_->weak();
  }

  int weak_ping_interval() const {
    return std::max(config_.ice_check_interval_weak_connectivity_or_default(),
                    config_.ice_check_min_interval_or_default());
  }

  int strong_ping_interval() const {
    return std::max(config_.ice_check_interval_strong_connectivity_or_default(),
                    config_.ice_check_min_interval_or_default());
  }

  int check_receiving_interval() const {
    return std::max(MIN_CHECK_RECEIVING_INTERVAL,
                    config_.receiving_timeout_or_default() / 10);
  }

  const Connection* FindOldestConnectionNeedingTriggeredCheck(int64_t now);
  // Between |conn1| and |conn2|, this function returns the one which should
  // be pinged first.
  const Connection* MorePingable(const Connection* conn1,
                                 const Connection* conn2);
  // Select the connection which is Relay/Relay. If both of them are,
  // UDP relay protocol takes precedence.
  const Connection* MostLikelyToWork(const Connection* conn1,
                                     const Connection* conn2);
  // Compare the last_ping_sent time and return the one least recently pinged.
  const Connection* LeastRecentlyPinged(const Connection* conn1,
                                        const Connection* conn2);

  bool IsPingable(const Connection* conn, int64_t now) const;
  bool IsBackupConnection(const Connection* conn) const;
  // Whether a writable connection is past its ping interval and needs to be
  // pinged again.
  bool WritableConnectionPastPingInterval(const Connection* conn,
                                          int64_t now) const;
  int CalculateActiveWritablePingInterval(const Connection* conn,
                                          int64_t now) const;

  std::map<rtc::Network*, const Connection*> GetBestConnectionByNetwork() const;
  std::vector<const Connection*> GetBestWritableConnectionPerNetwork() const;

  std::function<IceTransportState()> ice_transport_state_func_;
  std::function<rtc::ArrayView<const Connection*>()>
      sorted_connection_list_func_;
  IceConfig config_;
  const IceFieldTrials* field_trials_;

  // |connections_| is a sorted list with the first one always be the
  // |selected_connection_| when it's not nullptr. The combination of
  // |pinged_connections_| and |unpinged_connections_| has the same
  // connections as |connections_|. These 2 sets maintain whether a
  // connection should be pinged next or not.
  const Connection* selected_connection_ = nullptr;
  std::vector<const Connection*> connections_;
  std::set<const Connection*> pinged_connections_;
  std::set<const Connection*> unpinged_connections_;
};

}  // namespace cricket

#endif  // P2P_BASE_BASIC_ICE_CONTROLLER_H_
