/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_request.h"

#include <utility>
#include <vector>

#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace cricket {

class StunRequestTest : public ::testing::Test, public sigslot::has_slots<> {
 public:
  StunRequestTest()
      : manager_(rtc::Thread::Current()),
        request_count_(0),
        response_(NULL),
        success_(false),
        failure_(false),
        timeout_(false) {
    manager_.SignalSendPacket.connect(this, &StunRequestTest::OnSendPacket);
  }

  void OnSendPacket(const void* data, size_t size, StunRequest* req) {
    request_count_++;
  }

  void OnResponse(StunMessage* res) {
    response_ = res;
    success_ = true;
  }
  void OnErrorResponse(StunMessage* res) {
    response_ = res;
    failure_ = true;
  }
  void OnTimeout() { timeout_ = true; }

 protected:
  static std::unique_ptr<StunMessage> CreateStunMessage(
      StunMessageType type,
      const StunMessage* req) {
    std::unique_ptr<StunMessage> msg = std::make_unique<StunMessage>();
    msg->SetType(type);
    if (req) {
      msg->SetTransactionID(req->transaction_id());
    }
    return msg;
  }
  static int TotalDelay(int sends) {
    std::vector<int> delays = {0,    250,   750,   1750,  3750,
                               7750, 15750, 23750, 31750, 39750};
    return delays[sends];
  }

  StunRequestManager manager_;
  int request_count_;
  StunMessage* response_;
  bool success_;
  bool failure_;
  bool timeout_;
};

// Forwards results to the test class.
class StunRequestThunker : public StunRequest {
 public:
  StunRequestThunker(StunRequestManager* manager,
                     std::unique_ptr<StunMessage> msg,
                     StunRequestTest* test)
      : StunRequest(manager, std::move(msg)), test_(test) {}
  StunRequestThunker(StunRequestManager* manager, StunRequestTest* test)
      : StunRequest(manager), test_(test) {}

 private:
  virtual void OnResponse(StunMessage* res) { test_->OnResponse(res); }
  virtual void OnErrorResponse(StunMessage* res) {
    test_->OnErrorResponse(res);
  }
  virtual void OnTimeout() { test_->OnTimeout(); }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
  }

  StunRequestTest* test_;
};

// Test handling of a normal binding response.
TEST_F(StunRequestTest, TestSuccess) {
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, req.get());
  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test handling of an error binding response.
TEST_F(StunRequestTest, TestError) {
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_ERROR_RESPONSE, req.get());
  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_FALSE(success_);
  EXPECT_TRUE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test handling of a binding response with the wrong transaction id.
TEST_F(StunRequestTest, TestUnexpected) {
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, NULL);

  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  EXPECT_FALSE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test that requests are sent at the right times.
TEST_F(StunRequestTest, TestBackoff) {
  rtc::ScopedFakeClock fake_clock;
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, req.get());

  int64_t start = rtc::TimeMillis();
  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE_SIMULATED_WAIT(request_count_ != i, STUN_TOTAL_TIMEOUT,
                               fake_clock);
    int64_t elapsed = rtc::TimeMillis() - start;
    RTC_LOG(LS_INFO) << "STUN request #" << (i + 1) << " sent at " << elapsed
                     << " ms";
    EXPECT_EQ(TotalDelay(i), elapsed);
  }
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test that we timeout properly if no response is received.
TEST_F(StunRequestTest, TestTimeout) {
  rtc::ScopedFakeClock fake_clock;
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, req.get());

  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  SIMULATED_WAIT(false, cricket::STUN_TOTAL_TIMEOUT, fake_clock);

  EXPECT_FALSE(manager_.CheckResponse(res.get()));
  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_TRUE(timeout_);
}

// Regression test for specific crash where we receive a response with the
// same id as a request that doesn't have an underlying StunMessage yet.
TEST_F(StunRequestTest, TestNoEmptyRequest) {
  StunRequestThunker* request = new StunRequestThunker(&manager_, this);

  manager_.SendDelayed(request, 100);

  StunMessage dummy_req;
  dummy_req.SetTransactionID(request->id());
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, &dummy_req);

  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// If the response contains an attribute in the "comprehension required" range
// which is not recognized, the transaction should be considered a failure and
// the response should be ignored.
TEST_F(StunRequestTest, TestUnrecognizedComprehensionRequiredAttribute) {
  std::unique_ptr<StunMessage> req =
      CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_ERROR_RESPONSE, req.get());

  manager_.Send(new StunRequestThunker(&manager_, std::move(req), this));
  res->AddAttribute(StunAttribute::CreateUInt32(0x7777));
  EXPECT_FALSE(manager_.CheckResponse(res.get()));

  EXPECT_EQ(nullptr, response_);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

}  // namespace cricket
