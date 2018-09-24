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

#include "api/test/test_dependency_factory.h"

namespace webrtc {

std::unique_ptr<TestDependencyFactory> TestDependencyFactory::instance_ =
    nullptr;

const TestDependencyFactory& TestDependencyFactory::GetInstance() {
  if (instance_ == nullptr) {
    instance_ = absl::make_unique<TestDependencyFactory>();
  }
  return *instance_;
}

void TestDependencyFactory::SetInstance(
    std::unique_ptr<TestDependencyFactory> instance) {
  instance_ = std::move(instance);
}

std::unique_ptr<VideoQualityTestFixtureInterface::InjectionComponents>
TestDependencyFactory::CreateComponents() const {
  return nullptr;
}

}  // namespace webrtc
