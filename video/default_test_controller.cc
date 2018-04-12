/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/create_test_controller.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

class DefaultTestController : public TestControllerInterface {
 public:
  std::unique_ptr<FecController> CreateFecController() override;
  std::unique_ptr<test::LayerFilteringTransport>
      CreateSendTransport() override;
  std::unique_ptr<test::DirectTransport> CreateReceiveTransport() override;
};

std::unique_ptr<FecController>
DefaultFecControllerFactory::CreateFecController() {
  return nullptr;
}

std::unique_ptr<test::LayerFilteringTransport>
DefaultFecControllerFactory::CreateSendTransport() {
  return rtc::MakeUnique<test::LayerFilteringTransport>(
      &task_queue_, params_.pipe, sender_call_.get(), kPayloadTypeVP8,
      kPayloadTypeVP9, params_.video[0].selected_tl, params_.ss[0].selected_sl,
      payload_type_map_, kVideoSendSsrcs[0],
      static_cast<uint32_t>(kVideoSendSsrcs[0] + params_.ss[0].streams.size() -
                            1));
}

std::unique_ptr<test::DirectTransport>
DefaultFecControllerFactory::CreateReceiveTransport() {
  return rtc::MakeUnique<test::DirectTransport>(
      &task_queue_, params_.pipe, receiver_call_.get(), payload_type_map_);
}


std::unique_ptr<FecControllerFactoryInterface>
CreateTestController() {
  return rtc::MakeUnique<DefaultTestController>();
}

}  // namespace webrtc


