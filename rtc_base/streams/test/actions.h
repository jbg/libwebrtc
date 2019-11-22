/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TEST_ACTIONS_H_
#define RTC_BASE_STREAMS_TEST_ACTIONS_H_

#include <utility>

#include "test/gmock.h"

namespace webrtc {

template <typename T>
class ReadableStreamController;
template <typename T>
class WritableStreamController;
template <typename O>
class TransformStreamController;

template <typename F, typename... Args>
struct is_invocable
    : std::is_constructible<
          std::function<void(Args...)>,
          std::reference_wrapper<typename std::remove_reference<F>::type>> {};

template <typename Functor>
struct InvokeWithCompatibleArgAction {
  Functor functor;

  template <typename... Args>
  void operator()(Args... args) {
    constexpr int kArgIndex = GetIndex<Args...>();
    static_assert(kArgIndex >= 0, "");
    functor(std::get<kArgIndex>(std::make_tuple(args...)));
  }

  template <typename Arg>
  static constexpr int GetIndex() {
    if (is_invocable<Functor, Arg>::value) {
      return 0;
    }
    return INT_MIN;
  }

  template <typename Arg, typename Arg2, typename... Rest>
  static constexpr int GetIndex() {
    if (is_invocable<Functor, Arg>::value) {
      return 0;
    }
    return 1 + GetIndex<Arg2, Rest...>();
  }
};

template <typename Functor>
auto InvokeWithCompatibleArg(Functor functor) {
  return InvokeWithCompatibleArgAction<Functor>{std::move(functor)};
}

template <typename T>
auto StartAsync(ReadableStreamController<T>** controller_out) {
  return InvokeWithCompatibleArg([=](ReadableStreamController<T>* controller) {
    controller->StartAsync();
    *controller_out = controller;
  });
}

template <typename T>
auto StartAsync(WritableStreamController<T>** controller_out) {
  return InvokeWithCompatibleArg([=](WritableStreamController<T>* controller) {
    controller->StartAsync();
    *controller_out = controller;
  });
}

template <typename T>
auto StartAsync(TransformStreamController<T>** controller_out) {
  return InvokeWithCompatibleArg([=](TransformStreamController<T>* controller) {
    controller->StartAsync();
    *controller_out = controller;
  });
}

template <typename T>
struct WriteFunctor {
  T chunk;
  void operator()(ReadableStreamController<T>* controller) {
    ASSERT_TRUE(controller->IsWritable());
    controller->Write(std::move(chunk));
  }
  void operator()(TransformStreamController<T>* controller) {
    ASSERT_TRUE(controller->IsWritable());
    controller->Write(std::move(chunk));
  }
};

template <typename T>
auto Write(T chunk) {
  return InvokeWithCompatibleArg(WriteFunctor<T>{std::move(chunk)});
}

struct CloseFunctor {
  template <typename T>
  void operator()(ReadableStreamController<T>* controller) {
    controller->Close();
  }
  template <typename T>
  void operator()(WritableStreamController<T>* controller) {
    controller->Close();
  }
  template <typename T>
  void operator()(TransformStreamController<T>* controller) {
    controller->Close();
  }
};

inline auto Close() {
  return InvokeWithCompatibleArg(CloseFunctor{});
}

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TEST_ACTIONS_H_
