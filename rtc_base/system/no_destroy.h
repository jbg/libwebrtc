/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_NO_DESTROY_H_
#define RTC_BASE_SYSTEM_NO_DESTROY_H_

// RTC_NO_DESTROY is annotation to tell the compiler that a variable with static
// or thread storage duration shouldn't have its exit-time destructor run.
// Note that LSAN will happy with this annotation, so use this only in code that
// doesn't build with build_with_chromium=true.
#ifdef __has_attribute
#if __has_attribute(no_destroy)
#define RTC_NO_DESTROY __attribute__((no_destroy))
#endif
#endif
#ifndef RTC_NO_DESTROY
#define RTC_NO_DESTROY
#endif

#endif  // RTC_BASE_SYSTEM_NO_DESTROY_H_
