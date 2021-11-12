/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_CONSTINIT_H_
#define RTC_BASE_SYSTEM_CONSTINIT_H_

// RTC_CONSTINIT attribute specifies that the variable to which it is attached
// is intended to have a constant initializer.
#ifdef __has_attribute
#if __has_attribute(require_constant_initialization)
#define RTC_CONSTINIT __attribute__((require_constant_initialization))
#endif
#endif
#ifndef RTC_CONSTINIT
#define RTC_CONSTINIT
#endif

#endif  // RTC_BASE_SYSTEM_CONSTINIT_H_
