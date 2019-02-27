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

namespace rtc {

class Thread;

namespace post_message_with_functor_internal {

// Adds a layer of abstraction that allows moving the bulk of the
// PostMessageWithFunctor() implementation to the .cc in order to avoid a
// circular include dependency with rtc_base/thread.h.
void PostMessageWithFunctorImpl(const Location& posted_from,
                                Thread* thread,
                                MessageHandler* message_handler);

template <class FunctorT>
class SingleMessageHandlerWithFunctor : public MessageHandler {
 public:
  template <class F>
  explicit SingleMessageHandlerWithFunctor(F&& functor)
      : functor_(std::forward<F>(functor)) {}

  void OnMessage(Message* msg) override {
    functor_();
    delete this;
  }

 private:
  ~SingleMessageHandlerWithFunctor() override {}

  typename std::remove_reference<FunctorT>::type functor_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SingleMessageHandlerWithFunctor);
};

// Asynchronously posts a message that will invoke |functor| on the target
// thread. Ownership is passed and |functor| is destroyed on the target thread.
// Requirements of FunctorT:
// - FunctorT is movable.
// - FunctorT implements "T operator()()" or "T operator()() const" for some T
//   (if T is not void, the return value is discarded on the target thread).
// - FunctorT has a public destructor that can be invoked from the target
//   thread after operation() has been invoked.
// - The functor must not cause the thread to quit before
//   PostMessageWithFunctor() is done.
template <class FunctorT>
void PostMessageWithFunctor(const Location& posted_from,
                            Thread* thread,
                            FunctorT&& functor) {
  PostMessageWithFunctorImpl(
      posted_from, thread,
      new post_message_with_functor_internal::SingleMessageHandlerWithFunctor<
          FunctorT>(std::forward<FunctorT>(functor)));
}

}  // namespace post_message_with_functor_internal

}  // namespace rtc

#endif  // RTC_BASE_POST_MESSAGE_WITH_FUNCTOR_H_
