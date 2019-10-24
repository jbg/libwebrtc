/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_ice_controller.h"

#

namespace {

bool IsRelayRelay(const cricket::Connection* conn) {
  return conn->local_candidate().type() == cricket::RELAY_PORT_TYPE &&
         conn->remote_candidate().type() == cricket::RELAY_PORT_TYPE;
}

bool IsUdp(const cricket::Connection* conn) {
  return conn->local_candidate().relay_protocol() == cricket::UDP_PROTOCOL_NAME;
}

}  // namespace

namespace cricket {

BasicIceController::BasicIceController(
    std::function<IceTransportState()> ice_transport_state_func,
    std::function<rtc::ArrayView<const Connection*>()>
        sorted_connection_list_func,
    const IceFieldTrials* field_trials)
    : ice_transport_state_func_(ice_transport_state_func),
      sorted_connection_list_func_(sorted_connection_list_func),
      field_trials_(field_trials) {}

BasicIceController::~BasicIceController() {}

void BasicIceController::SetIceConfig(const IceConfig& config) {
  config_ = config;
}

void BasicIceController::SetSelectedConnection(
    const Connection* selected_connection) {
  selected_connection_ = selected_connection;
}

void BasicIceController::AddConnection(const Connection* connection) {
  connections_.push_back(connection);
  unpinged_connections_.insert(connection);
}

void BasicIceController::OnConnectionDestroyed(const Connection* connection) {
  pinged_connections_.erase(connection);
  unpinged_connections_.erase(connection);
  connections_.erase(absl::c_find(connections_, connection));
}

bool BasicIceController::HasPingableConnection() const {
  int64_t now = rtc::TimeMillis();
  return absl::c_any_of(connections_, [this, now](const Connection* c) {
    return IsPingable(c, now);
  });
}

std::pair<Connection*, int> BasicIceController::SelectConnectionToPing(
    int64_t last_ping_sent_ms) {
  // When the selected connection is not receiving or not writable, or any
  // active connection has not been pinged enough times, use the weak ping
  // interval.
  bool need_more_pings_at_weak_interval =
      absl::c_any_of(connections_, [](const Connection* conn) {
        return conn->active() &&
               conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL;
      });
  int ping_interval = (weak() || need_more_pings_at_weak_interval)
                          ? weak_ping_interval()
                          : strong_ping_interval();

  const Connection* conn = nullptr;
  if (rtc::TimeMillis() >= last_ping_sent_ms + ping_interval) {
    conn = FindNextPingableConnection();
  }
  int delay = std::min(ping_interval, check_receiving_interval());
  return std::make_pair(const_cast<Connection*>(conn), delay);
}

void BasicIceController::MarkConnectionPinged(const Connection* conn) {
  if (conn && pinged_connections_.insert(conn).second) {
    unpinged_connections_.erase(conn);
  }
}

// Returns the next pingable connection to ping.
const Connection* BasicIceController::FindNextPingableConnection() {
  int64_t now = rtc::TimeMillis();

  // Rule 1: Selected connection takes priority over non-selected ones.
  if (selected_connection_ && selected_connection_->connected() &&
      selected_connection_->writable() &&
      WritableConnectionPastPingInterval(selected_connection_, now)) {
    return selected_connection_;
  }

  // Rule 2: If the channel is weak, we need to find a new writable and
  // receiving connection, probably on a different network. If there are lots of
  // connections, it may take several seconds between two pings for every
  // non-selected connection. This will cause the receiving state of those
  // connections to be false, and thus they won't be selected. This is
  // problematic for network fail-over. We want to make sure at least one
  // connection per network is pinged frequently enough in order for it to be
  // selectable. So we prioritize one connection per network.
  // Rule 2.1: Among such connections, pick the one with the earliest
  // last-ping-sent time.
  if (weak()) {
    std::vector<const Connection*> pingable_selectable_connections;
    absl::c_copy_if(GetBestWritableConnectionPerNetwork(),
                    std::back_inserter(pingable_selectable_connections),
                    [this, now](const Connection* conn) {
                      return WritableConnectionPastPingInterval(conn, now);
                    });
    auto iter = absl::c_min_element(
        pingable_selectable_connections,
        [](const Connection* conn1, const Connection* conn2) {
          return conn1->last_ping_sent() < conn2->last_ping_sent();
        });
    if (iter != pingable_selectable_connections.end()) {
      return *iter;
    }
  }

  // Rule 3: Triggered checks have priority over non-triggered connections.
  // Rule 3.1: Among triggered checks, oldest takes precedence.
  const Connection* oldest_triggered_check =
      FindOldestConnectionNeedingTriggeredCheck(now);
  if (oldest_triggered_check) {
    return oldest_triggered_check;
  }

  // Rule 4: Unpinged connections have priority over pinged ones.
  RTC_CHECK(connections_.size() ==
            pinged_connections_.size() + unpinged_connections_.size());
  // If there are unpinged and pingable connections, only ping those.
  // Otherwise, treat everything as unpinged.
  // TODO(honghaiz): Instead of adding two separate vectors, we can add a state
  // "pinged" to filter out unpinged connections.
  if (absl::c_none_of(unpinged_connections_,
                      [this, now](const Connection* conn) {
                        return this->IsPingable(conn, now);
                      })) {
    unpinged_connections_.insert(pinged_connections_.begin(),
                                 pinged_connections_.end());
    pinged_connections_.clear();
  }

  // Among un-pinged pingable connections, "more pingable" takes precedence.
  std::vector<const Connection*> pingable_connections;
  absl::c_copy_if(
      unpinged_connections_, std::back_inserter(pingable_connections),
      [this, now](const Connection* conn) { return IsPingable(conn, now); });
  auto iter = absl::c_max_element(
      pingable_connections,
      [this](const Connection* conn1, const Connection* conn2) {
        // Some implementations of max_element
        // compare an element with itself.
        if (conn1 == conn2) {
          return false;
        }
        return MorePingable(conn1, conn2) == conn2;
      });
  if (iter != pingable_connections.end()) {
    return *iter;
  }
  return nullptr;
}

// Find "triggered checks".  We ping first those connections that have
// received a ping but have not sent a ping since receiving it
// (last_ping_received > last_ping_sent).  But we shouldn't do
// triggered checks if the connection is already writable.
const Connection* BasicIceController::FindOldestConnectionNeedingTriggeredCheck(
    int64_t now) {
  const Connection* oldest_needing_triggered_check = nullptr;
  for (auto* conn : connections_) {
    if (!IsPingable(conn, now)) {
      continue;
    }
    bool needs_triggered_check =
        (!conn->writable() &&
         conn->last_ping_received() > conn->last_ping_sent());
    if (needs_triggered_check &&
        (!oldest_needing_triggered_check ||
         (conn->last_ping_received() <
          oldest_needing_triggered_check->last_ping_received()))) {
      oldest_needing_triggered_check = conn;
    }
  }

  if (oldest_needing_triggered_check) {
    RTC_LOG(LS_INFO) << "Selecting connection for triggered check: "
                     << oldest_needing_triggered_check->ToString();
  }
  return oldest_needing_triggered_check;
}

bool BasicIceController::WritableConnectionPastPingInterval(
    const Connection* conn,
    int64_t now) const {
  int interval = CalculateActiveWritablePingInterval(conn, now);
  return conn->last_ping_sent() + interval <= now;
}

int BasicIceController::CalculateActiveWritablePingInterval(
    const Connection* conn,
    int64_t now) const {
  // Ping each connection at a higher rate at least
  // MIN_PINGS_AT_WEAK_PING_INTERVAL times.
  if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
    return weak_ping_interval();
  }

  int stable_interval =
      config_.stable_writable_connection_ping_interval_or_default();
  int weak_or_stablizing_interval = std::min(
      stable_interval, WEAK_OR_STABILIZING_WRITABLE_CONNECTION_PING_INTERVAL);
  // If the channel is weak or the connection is not stable yet, use the
  // weak_or_stablizing_interval.
  return (!weak() && conn->stable(now)) ? stable_interval
                                        : weak_or_stablizing_interval;
}

// Is the connection in a state for us to even consider pinging the other side?
// We consider a connection pingable even if it's not connected because that's
// how a TCP connection is kicked into reconnecting on the active side.
bool BasicIceController::IsPingable(const Connection* conn, int64_t now) const {
  const Candidate& remote = conn->remote_candidate();
  // We should never get this far with an empty remote ufrag.
  RTC_DCHECK(!remote.username().empty());
  if (remote.username().empty() || remote.password().empty()) {
    // If we don't have an ICE ufrag and pwd, there's no way we can ping.
    return false;
  }

  // A failed connection will not be pinged.
  if (conn->state() == IceCandidatePairState::FAILED) {
    return false;
  }

  // An never connected connection cannot be written to at all, so pinging is
  // out of the question. However, if it has become WRITABLE, it is in the
  // reconnecting state so ping is needed.
  if (!conn->connected() && !conn->writable()) {
    return false;
  }

  // If we sent a number of pings wo/ reply, skip sending more
  // until we get one.
  if (conn->TooManyOutstandingPings(field_trials_->max_outstanding_pings)) {
    return false;
  }

  // If the channel is weakly connected, ping all connections.
  if (weak()) {
    return true;
  }

  // Always ping active connections regardless whether the channel is completed
  // or not, but backup connections are pinged at a slower rate.
  if (IsBackupConnection(conn)) {
    return conn->rtt_samples() == 0 ||
           (now >= conn->last_ping_response_received() +
                       config_.backup_connection_ping_interval_or_default());
  }
  // Don't ping inactive non-backup connections.
  if (!conn->active()) {
    return false;
  }

  // Do ping unwritable, active connections.
  if (!conn->writable()) {
    return true;
  }

  // Ping writable, active connections if it's been long enough since the last
  // ping.
  return WritableConnectionPastPingInterval(conn, now);
}

// A connection is considered a backup connection if the channel state
// is completed, the connection is not the selected connection and it is active.
bool BasicIceController::IsBackupConnection(const Connection* conn) const {
  return ice_transport_state_func_() == IceTransportState::STATE_COMPLETED &&
         conn != selected_connection_ && conn->active();
}

const Connection* BasicIceController::MorePingable(const Connection* conn1,
                                                   const Connection* conn2) {
  RTC_DCHECK(conn1 != conn2);
  if (config_.prioritize_most_likely_candidate_pairs) {
    const Connection* most_likely_to_work_conn = MostLikelyToWork(conn1, conn2);
    if (most_likely_to_work_conn) {
      return most_likely_to_work_conn;
    }
  }

  const Connection* least_recently_pinged_conn =
      LeastRecentlyPinged(conn1, conn2);
  if (least_recently_pinged_conn) {
    return least_recently_pinged_conn;
  }

  // During the initial state when nothing has been pinged yet, return the first
  // one in the ordered |connections_|.
  auto connections = sorted_connection_list_func_();
  return *(std::find_if(connections.begin(), connections.end(),
                        [conn1, conn2](const Connection* conn) {
                          return conn == conn1 || conn == conn2;
                        }));
}

const Connection* BasicIceController::MostLikelyToWork(
    const Connection* conn1,
    const Connection* conn2) {
  bool rr1 = IsRelayRelay(conn1);
  bool rr2 = IsRelayRelay(conn2);
  if (rr1 && !rr2) {
    return conn1;
  } else if (rr2 && !rr1) {
    return conn2;
  } else if (rr1 && rr2) {
    bool udp1 = IsUdp(conn1);
    bool udp2 = IsUdp(conn2);
    if (udp1 && !udp2) {
      return conn1;
    } else if (udp2 && udp1) {
      return conn2;
    }
  }
  return nullptr;
}

const Connection* BasicIceController::LeastRecentlyPinged(
    const Connection* conn1,
    const Connection* conn2) {
  if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
    return conn1;
  }
  if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
    return conn2;
  }
  return nullptr;
}

std::map<rtc::Network*, const Connection*>
BasicIceController::GetBestConnectionByNetwork() const {
  // |connections_| has been sorted, so the first one in the list on a given
  // network is the best connection on the network, except that the selected
  // connection is always the best connection on the network.
  std::map<rtc::Network*, const Connection*> best_connection_by_network;
  if (selected_connection_) {
    best_connection_by_network[selected_connection_->port()->Network()] =
        selected_connection_;
  }
  // TODO(honghaiz): Need to update this if |connections_| are not sorted.
  for (const Connection* conn : sorted_connection_list_func_()) {
    rtc::Network* network = conn->port()->Network();
    // This only inserts when the network does not exist in the map.
    best_connection_by_network.insert(std::make_pair(network, conn));
  }
  return best_connection_by_network;
}

std::vector<const Connection*>
BasicIceController::GetBestWritableConnectionPerNetwork() const {
  std::vector<const Connection*> connections;
  for (auto kv : GetBestConnectionByNetwork()) {
    const Connection* conn = kv.second;
    if (conn->writable() && conn->connected()) {
      connections.push_back(conn);
    }
  }
  return connections;
}

}  // namespace cricket
