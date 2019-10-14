/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPERATIONS_CHAIN_H_
#define RTC_BASE_OPERATIONS_CHAIN_H_

#include <functional>
#include <list>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace rtc {

class OperationsChain;

// The OperationsChainCallback must be invoked by an operation running on an
// OperationsChain exactly once to indicate that the operation has completed,
// allowing subsequent operations to run on the chain.
class OperationsChainCallback : public RefCountedObject<RefCountInterface> {
 public:
  void OnOperationComplete();

 private:
  friend class OperationsChain;

  static scoped_refptr<OperationsChainCallback> Create(
      scoped_refptr<OperationsChain> operations_chain);

  explicit OperationsChainCallback(
      scoped_refptr<OperationsChain> operations_chain);
  ~OperationsChainCallback();

  scoped_refptr<OperationsChain> operations_chain_;
  bool has_run_ = false;

  RTC_DISALLOW_COPY_AND_ASSIGN(OperationsChainCallback);
};

namespace rtc_operations_chain_internal {

// Abstract base class for operations on the OperationsChain. Run() must be
// invoked exactly once during the Operation's lifespan.
class Operation {
 public:
  virtual ~Operation() {}

  virtual void Run() = 0;
};

// FunctorT is the same as in OperationsChain::ChainOperation(). |callback_| is
// passed on to the |functor_| and is used to inform the OperationsChain that
// the operation completed. The functor is responsible for invoking the
// callback when the operation has completed.
template <typename FunctorT>
class OperationWithFunctor : public Operation {
 public:
  OperationWithFunctor(FunctorT&& functor,
                       scoped_refptr<OperationsChainCallback> callback)
      : functor_(std::forward<FunctorT>(functor)),
        callback_(std::move(callback)) {}

  ~OperationWithFunctor() override { RTC_DCHECK(has_run_); }

  void Run() override {
    RTC_DCHECK(!has_run_);
#ifdef RTC_DCHECK_IS_ON
    has_run_ = true;
#endif  // RTC_DCHECK_IS_ON
    functor_(std::move(callback_));
  }

 private:
  typename std::remove_reference<FunctorT>::type functor_;
  scoped_refptr<OperationsChainCallback> callback_;
#ifdef RTC_DCHECK_IS_ON
  bool has_run_ = false;
#endif  // RTC_DCHECK_IS_ON
};

}  // namespace rtc_operations_chain_internal

// An implementation of an operations chain. An operations chain is used to
// ensure that asynchronous tasks are executed in-order with at most one task
// running at a time. The notion of an operation chain is defined in
// https://w3c.github.io/webrtc-pc/#dfn-operations-chain, though unlike this
// implementation, the referenced definition is coupled with a peer connection.
//
// An operation is an asynchronous task. The operation starts when its functor
// is invoked, and completes when the callback that is passed to functor is
// invoked by the operation. The operation must start and complete on the same
// sequence that the operation was "chained" on. As such, the OperationsChain
// operates in a "single-threaded" fashion, but the asynchronous operations may
// use any number of threads to achieve "in parallel" behavior.
//
// When an operation is chained onto the OperationsChain, it is enqueued to be
// executed. Operations are executed in FIFO order, where the next operation
// does not start until the previous operation has completed. OperationsChain
// guarantees that:
// - If the operations chain is empty when an operation is chained, the
//   operation starts immediately, inside ChainOperation().
// - If the operations chain is not empty when an operation is chained, the
//   operation starts upon the previous operation completing, inside the
//   callback.
//
// An operation is contractually obligated to invoke the completion callback
// exactly once. Cancelling a chained operation is not supported by the
// OperationsChain; an operation that wants to be cancellable is responsible for
// aborting its own steps. The callback must still be invoked.
//
// The OperationsChain is kept-alive through reference counting if there are
// operations pending. This, together with the contract, guarantees that all
// operations that are chained get executed.
class OperationsChain : public RefCountedObject<RefCountInterface> {
 public:
  static scoped_refptr<OperationsChain> Create();
  ~OperationsChain();

  // Chains an operation. Chained operations are executed in FIFO order. The
  // operation starts when |functor| is executed by the OperationsChain and is
  // contractually obligated to invoke the callback passed to it when the
  // operation is complete. Operations must start and complete on the same
  // sequence that this method was invoked on.
  //
  // If the OperationsChain is empty, the operation starts immediately.
  // Otherwise it starts upon the previous operation completing.
  //
  // Requirements of FunctorT:
  // - FunctorT is movable.
  // - FunctorT implements "T operator()(std::function<void()> callback)" or
  //   "T operator()(std::function<void()> callback) const" for some T (if T is
  //   not void, the return value is discarded in the invoking sequence). The
  //   operator starts the operation; when the operation is complete, "callback"
  //   MUST be invoked, and it MUST be so on the sequence that ChainOperation()
  //   was invoked on.
  //
  // Lambda expressions are valid functors.
  template <typename FunctorT>
  void ChainOperation(FunctorT&& functor) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    chained_operations_.push_back(
        std::unique_ptr<rtc_operations_chain_internal::Operation>(
            new rtc_operations_chain_internal::OperationWithFunctor<FunctorT>(
                std::forward<FunctorT>(functor),
                OperationsChainCallback::Create(this))));
    // If this is the only operation in the chain we execute it immediately.
    // Otherwise the callback will get invoked when the pending operation
    // completes which will trigger the next operation to execute.
    if (chained_operations_.size() == 1) {
      chained_operations_.front()->Run();
    }
  }

 private:
  friend class OperationsChainCallback;

  // All operations MUST start and complete on the sequence that OperationsChain
  // is constructed on.
  OperationsChain();

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

  webrtc::SequenceChecker sequence_checker_;
  // FIFO-list of operations that are chained. An operation that is executing
  // remains on this list until it has completed by invoking the callback passed
  // to it.
  std::list<std::unique_ptr<rtc_operations_chain_internal::Operation>>
      chained_operations_ RTC_GUARDED_BY(sequence_checker_);

  RTC_DISALLOW_COPY_AND_ASSIGN(OperationsChain);
};

}  // namespace rtc

#endif  // RTC_BASE_OPERATIONS_CHAIN_H_
