/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/post_message_with_functor.h"

#include <memory>

#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace rtc {

class PostMessageWithFunctorTest : public testing::Test {
 public:
  PostMessageWithFunctorTest() : background_thread_(rtc::Thread::Create()) {
    background_thread_->Start();
  }

  void WaitAndSetEventOnBackgroundThread(Event* wait_event, Event* set_event) {
    RTC_DCHECK(background_thread_->IsCurrent());
    wait_event->Wait(Event::kForever);
    set_event->Set();
  }

 protected:
  std::unique_ptr<rtc::Thread> background_thread_;
};

TEST_F(PostMessageWithFunctorTest, InvokesFunctorAsynchronously) {
  // The first event ensures that SendSingleMessage() is not blocking this
  // thread. The second event ensures that the message is processed.
  Event event_set_by_test_thread;
  Event event_set_by_background_thread;
  PostMessageWithFunctor(
      RTC_FROM_HERE, background_thread_.get(),
      Bind(&PostMessageWithFunctorTest::WaitAndSetEventOnBackgroundThread,
           static_cast<PostMessageWithFunctorTest*>(this),
           &event_set_by_test_thread, &event_set_by_background_thread));
  event_set_by_test_thread.Set();
  event_set_by_background_thread.Wait(Event::kForever);
}

TEST_F(PostMessageWithFunctorTest, InvokesInPostedOrder) {
  Event first;
  Event second;
  Event third;
  Event fourth;

  PostMessageWithFunctor(
      RTC_FROM_HERE, background_thread_.get(),
      Bind(&PostMessageWithFunctorTest::WaitAndSetEventOnBackgroundThread,
           static_cast<PostMessageWithFunctorTest*>(this), &first, &second));
  PostMessageWithFunctor(
      RTC_FROM_HERE, background_thread_.get(),
      Bind(&PostMessageWithFunctorTest::WaitAndSetEventOnBackgroundThread,
           static_cast<PostMessageWithFunctorTest*>(this), &second, &third));
  PostMessageWithFunctor(
      RTC_FROM_HERE, background_thread_.get(),
      Bind(&PostMessageWithFunctorTest::WaitAndSetEventOnBackgroundThread,
           static_cast<PostMessageWithFunctorTest*>(this), &third, &fourth));

  // All tasks have been posted before the first one is unblocked.
  first.Set();
  // Only if the chain is invoked in posted order will the last event be set.
  fourth.Wait(Event::kForever);
}

}  // namespace rtc
