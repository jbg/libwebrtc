/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/nethelpers.h"
#include "rtc_base/gunit.h"

namespace rtc {

class AsyncResolverTest : public testing::Test, public sigslot::has_slots<> {
 public:
  ~AsyncResolverTest() override = default;
  bool resolved() const { return resolved_; }

  void OnResolved(AsyncResolverInterface*) { resolved_ = true; }

  void TestResolve() {
    // TODO(zstein): We could inject a fake getaddrinfo here.
    AsyncResolver* resolver = new AsyncResolver;
    AsyncResolver* resolver_alias = resolver;  // Does not increment ref count.
    resolver->SignalDone.connect(this, &AsyncResolverTest::OnResolved);
    SocketAddress address("google.com", 80);
    resolver->Start(address);
    EXPECT_TRUE_WAIT(resolved(), 10000);
    // resolver will Destroy itself
    resolver->Destroy(true);
    resolver_alias->Destroy(false);
  }

 private:
  bool resolved_ = false;
};

TEST_F(AsyncResolverTest, CheckForLeak) {
  TestResolve();
}

struct SimpleClass {
  int y_ = 0;
};

TEST_F(AsyncResolverTest, TestLeakDetection) {
  auto* x = new SimpleClass;
  std::cout << x->y_ << std::endl;
  // TODO(zstein): x should leak - delete is not called.
}

TEST_F(AsyncResolverTest, TestUseAfterFree) {
  SimpleClass* x = new SimpleClass();
  delete x;

  std::cout << x->y_ << std::endl;
}

}  // namespace rtc
