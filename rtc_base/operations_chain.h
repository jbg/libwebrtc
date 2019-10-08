/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPERATIONS_CHAIN_
#define RTC_BASE_OPERATIONS_CHAIN_

#include "rtc_base/constructor_magic.h"

#include <functional>
#include <list>
#include <memory>
#include <type_traits>

#include "api/scoped_refptr.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace rtc {

namespace rtc_operations_chain_internal {

class Operation {
 public:
  Operation() {}
  virtual ~Operation() {}

  virtual void Run() = 0;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(Operation);
};

template <typename FunctorT>
class OperationWithFunctor : public Operation {
 public:
  OperationWithFunctor(FunctorT&& functor, std::function<void()> callback)
      : functor_(std::forward<FunctorT>(functor)),
        callback_(std::move(callback)) {}

  ~OperationWithFunctor() override {}

  void Run() override { functor_(std::move(callback_)); }

  // TODO: Lifetime etc!

 private:
  typename std::remove_reference<FunctorT>::type functor_;
  std::function<void()> callback_;
};

}  // namespace rtc_operations_chain_internal

// TODO(hbos): Document this garbage.
// An operation needs to either keep this object alive or be able to tell if
// this object is dead or else the callback is not safe.
class OperationsChain : public rtc::RefCountInterface {
 public:
  static rtc::scoped_refptr<OperationsChain> Create() {
    return new rtc::RefCountedObject<OperationsChain>();
  }

  OperationsChain() { RTC_DCHECK_RUN_ON(&sequence_checker_); }
  // The OperationsChain is allowed to be destroyed on a different thread than
  // it was constructed on.
  ~OperationsChain() {
    // Operations keep the chain alive through reference counting so this should
    // not be possible.
    RTC_DCHECK(chained_operations_.empty());
  }

  // TODO: blah blah
  // Requirements of FunctorT:
  // - FunctorT is movable.
  // - FunctorT implements "T operator()(std::function<void()> callback)" or
  //   "T operator()(std::function<void()> callback) const" for some T (if T is
  //   not void, the return value is discarded in the invoking thread). The
  //   operator starts the operation; when the operation is complete, "callback"
  //   MUST be invoked on the sequence that ChainOperation() was invoked.
  template <typename FunctorT>
  void ChainOperation(FunctorT&& functor) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    std::unique_ptr<rtc_operations_chain_internal::Operation> operation(
        new rtc_operations_chain_internal::OperationWithFunctor<FunctorT>(
            std::forward<FunctorT>(functor), OperationCallback()));
    chained_operations_.push_back(std::move(operation));
    // If this is the only operation in the chain we execute it immediately.
    // Otherwise the OperationCallback() will get invoked when the pending
    // operation completes which will trigger the next operation to execute.
    if (chained_operations_.size() == 1) {
      chained_operations_.front()->Run();
    }
  }

 private:
  std::function<void()> OperationCallback() {
    // The callback keeps |this| alive until the operation has completed.
    return [this_ref = rtc::scoped_refptr<OperationsChain>(this)]() {
      // TODO(hbos): Invoking "callback" twice should cause a crash as it would
      // break other assumptions.
      this_ref->OnOperationComplete();
    };
  }

  void OnOperationComplete() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    // The front element is the operation that just completed, remove it.
    RTC_DCHECK(!chained_operations_.empty());
    chained_operations_.pop_front();
    // If there are any other operations chained, execute the next one.
    if (!chained_operations_.empty()) {
      chained_operations_.front()->Run();
    }
  }

  // Used to ensure all operations start and complete on the same thread that
  // the OperationsChain completed on.
  webrtc::SequenceChecker sequence_checker_;
  // FIFO-list of operations that are chained.
  std::list<std::unique_ptr<rtc_operations_chain_internal::Operation>>
      chained_operations_;

  RTC_DISALLOW_COPY_AND_ASSIGN(OperationsChain);
};

}  // namespace rtc

#endif  // RTC_BASE_OPERATIONS_CHAIN_
