/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/operations_chain.h"

#include "rtc_base/checks.h"

namespace rtc {

// static
scoped_refptr<OperationsChainCallback> OperationsChainCallback::Create(
    scoped_refptr<OperationsChain> operations_chain) {
  return new OperationsChainCallback(std::move(operations_chain));
}

OperationsChainCallback::OperationsChainCallback(
    scoped_refptr<OperationsChain> operations_chain)
    : operations_chain_(std::move(operations_chain)) {}

OperationsChainCallback::~OperationsChainCallback() {
  RTC_DCHECK(has_run_);
}

void OperationsChainCallback::OnOperationComplete() {
  RTC_DCHECK(!has_run_);
#ifdef RTC_DCHECK_IS_ON
  has_run_ = true;
#endif  // RTC_DCHECK_IS_ON
  operations_chain_->OnOperationComplete();
}

// static
scoped_refptr<OperationsChain> OperationsChain::Create() {
  return new OperationsChain();
}

OperationsChain::OperationsChain() : RefCountedObject() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

OperationsChain::~OperationsChain() {
  // Operations keep the chain alive through reference counting so this should
  // not be possible. The fact that the chain is empty makes it safe to
  // destroy the OperationsChain on any sequence.
  RTC_DCHECK(chained_operations_.empty());
}

void OperationsChain::OnOperationComplete() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // The front element is the operation that just completed, remove it.
  RTC_DCHECK(!chained_operations_.empty());
  chained_operations_.pop_front();
  // If there are any other operations chained, execute the next one.
  if (!chained_operations_.empty()) {
    chained_operations_.front()->Run();
  }
}

}  // namespace rtc
