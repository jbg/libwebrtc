/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MOCK_ACTIVE_ICE_CONTROLLER_H_
#define P2P_BASE_MOCK_ACTIVE_ICE_CONTROLLER_H_

#include <memory>

#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/active_ice_controller_interface.h"
#include "test/gmock.h"

namespace cricket {

class MockActiveIceController : public cricket::ActiveIceControllerInterface {
 public:
  explicit MockActiveIceController(
      const cricket::ActiveIceControllerFactoryArgs& args) {}
  ~MockActiveIceController() override = default;

  MOCK_METHOD(void, SetIceConfig, (const cricket::IceConfig&), (override));
  MOCK_METHOD(void,
              OnConnectionAdded,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionSwitched,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionDestroyed,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionPinged,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionReport,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(rtc::ArrayView<const cricket::Connection*>,
              Connections,
              (),
              (const, override));
  MOCK_METHOD(bool,
              GetUseCandidateAttribute,
              (const cricket::Connection*,
               cricket::NominationMode,
               cricket::IceMode),
              (const, override));
  MOCK_METHOD(void,
              OnSortAndSwitchRequest,
              (cricket::IceSwitchReason),
              (override));
  MOCK_METHOD(void,
              OnImmediateSortAndSwitchRequest,
              (cricket::IceSwitchReason),
              (override));
  MOCK_METHOD(bool,
              OnImmediateSwitchRequest,
              (cricket::IceSwitchReason, const cricket::Connection*),
              (override));
  MOCK_METHOD(const cricket::Connection*,
              FindNextPingableConnection,
              (),
              (override));
};

class MockActiveIceControllerFactory
    : public cricket::ActiveIceControllerFactoryInterface {
 public:
  ~MockActiveIceControllerFactory() override = default;

  std::unique_ptr<cricket::ActiveIceControllerInterface> Create(
      const cricket::ActiveIceControllerFactoryArgs& args) {
    RecordActiveIceControllerCreated();
    std::unique_ptr<MockActiveIceController> instance =
        std::make_unique<MockActiveIceController>(args);
    most_recent_instance_ = instance.get();
    return instance;
  }

  const MockActiveIceController* most_recent_instance() const {
    return most_recent_instance_;
  }

  MOCK_METHOD(void, RecordActiveIceControllerCreated, ());

 private:
  const MockActiveIceController* most_recent_instance_;
};

}  // namespace cricket

#endif  // P2P_BASE_MOCK_ACTIVE_ICE_CONTROLLER_H_
