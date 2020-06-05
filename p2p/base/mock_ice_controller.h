/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MOCK_ICE_CONTROLLER_H_
#define P2P_BASE_MOCK_ICE_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "p2p/base/ice_controller_interface.h"

namespace cricket {

// This is a trivial adapter that is intended to be used to selectively MOCK
// methods. Create a Mock class and mock methods of interest.
class MockIceController : public IceControllerInterface {
 public:
  explicit MockIceController(std::unique_ptr<IceControllerInterface> impl)
      : impl_(std::move(impl)) {}

  void SetIceConfig(const IceConfig& config) override {
    impl_->SetIceConfig(config);
  }
  void SetSelectedConnection(const Connection* selected_connection) override {
    impl_->SetSelectedConnection(selected_connection);
  }
  void AddConnection(const Connection* connection) override {
    impl_->AddConnection(connection);
  }
  void OnConnectionDestroyed(const Connection* connection) override {
    impl_->OnConnectionDestroyed(connection);
  }

  rtc::ArrayView<const Connection*> connections() const override {
    return impl_->connections();
  }
  bool HasPingableConnection() const override {
    return impl_->HasPingableConnection();
  }
  PingResult SelectConnectionToPing(int64_t last_ping_sent_ms) override {
    return impl_->SelectConnectionToPing(last_ping_sent_ms);
  }
  bool GetUseCandidateAttr(const Connection* conn,
                           NominationMode mode,
                           IceMode remote_ice_mode) const override {
    return impl_->GetUseCandidateAttr(conn, mode, remote_ice_mode);
  }

  const Connection* FindNextPingableConnection() override {
    return impl_->FindNextPingableConnection();
  }
  void MarkConnectionPinged(const Connection* con) override {
    return impl_->MarkConnectionPinged(con);
  }
  SwitchResult ShouldSwitchConnection(IceControllerEvent reason,
                                      const Connection* connection) override {
    return impl_->ShouldSwitchConnection(reason, connection);
  }
  SwitchResult SortAndSwitchConnection(IceControllerEvent reason) override {
    return impl_->SortAndSwitchConnection(reason);
  }
  std::vector<const Connection*> PruneConnections() override {
    return impl_->PruneConnections();
  }

 protected:
  std::unique_ptr<IceControllerInterface> impl_;
};

template <class T>
class MockIceControllerFactory : public cricket::IceControllerFactoryInterface {
 public:
  std::unique_ptr<cricket::IceControllerInterface> Create(
      const cricket::IceControllerFactoryArgs& args) override {
    std::unique_ptr<IceControllerInterface> controller =
        std::make_unique<cricket::BasicIceController>(args);
    auto mock = std::make_unique<T>(std::move(controller));
    // Keep a pointer to allow modifying calls.
    // Must not be used after the p2ptransportchannel has been destructed.
    controller_ = mock.get();
    return mock;
  }
  virtual ~MockIceControllerFactory() = default;

  T* controller_;
};

}  // namespace cricket

#endif  // P2P_BASE_MOCK_ICE_CONTROLLER_H_
