/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_POST_MESSAGE_WITH_FUNCTOR_H_
#define RTC_BASE_POST_MESSAGE_WITH_FUNCTOR_H_

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/location.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/thread.h"

namespace rtc {

namespace post_message_with_functor_internal {

template <class FunctorT>
class SingleMessageHandlerWithFunctor : public MessageHandler {
 public:
  explicit SingleMessageHandlerWithFunctor(FunctorT&& functor)
      : functor_(std::move(functor)) {}

  void OnMessage(Message* msg) override {
    functor_();
    delete this;
  }

 private:
  ~SingleMessageHandlerWithFunctor() override {}

  FunctorT functor_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SingleMessageHandlerWithFunctor);
};

}  // namespace post_message_with_functor_internal

// Asynchronously posts a message that will invoke |functor| on the target
// thread. Ownership is passed and |functor| is destroyed on the target thread.
template <class FunctorT>
static void PostMessageWithFunctor(const Location& posted_from,
                                   Thread* thread,
                                   FunctorT&& functor) {
  thread->Post(
      posted_from,
      new post_message_with_functor_internal::SingleMessageHandlerWithFunctor<
          FunctorT>(std::move(functor)));
}

}  // namespace rtc

#endif  // RTC_BASE_POST_MESSAGE_WITH_FUNCTOR_H_
