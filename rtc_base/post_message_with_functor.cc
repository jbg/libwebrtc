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

#include "rtc_base/thread.h"

namespace rtc {

namespace post_message_with_functor_internal {

void PostMessageWithFunctorImpl(const Location& posted_from,
                                Thread* thread,
                                MessageHandler* message_handler) {
  thread->Post(posted_from, message_handler);
  // This DCHECK guarantees that the post was successful.
  // Post() doesn't say whether it succeeded, but it will only fail if the
  // thread is quitting. DCHECKing that the thread is not quitting *after*
  // posting might yield some false positives (where the thread did in fact
  // quit, but only after posting), but if we have false positives here then we
  // have a race condition anyway.
  RTC_DCHECK(!thread->IsQuitting());
}

}  // namespace post_message_with_functor_internal

}  // namespace rtc
