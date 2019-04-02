/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_MOD_OPS_H_
#define RTC_BASE_NUMERICS_MOD_OPS_H_

#include <algorithm>
#include <type_traits>

#include "rtc_base/checks.h"

namespace webrtc {

template <unsigned long M>                                    // NOLINT
inline unsigned long Add(unsigned long a, unsigned long b) {  // NOLINT
  RTC_DCHECK_LT(a, M);
  unsigned long t = M - b % M;  // NOLINT
  unsigned long res = a - t;    // NOLINT
  if (t > a)
    return res + M;
  return res;
}

template <unsigned long M>                                         // NOLINT
inline unsigned long Subtract(unsigned long a, unsigned long b) {  // NOLINT
  RTC_DCHECK_LT(a, M);
  unsigned long sub = b % M;  // NOLINT
  if (a < sub)
    return M - (sub - a);
  return a - sub;
}

// Calculates the forward difference between two wrapping numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ForwardDiff(x, y) == 5
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          |----->----->----->----->----->
//
// ForwardDiff(y, x) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// -->----->                              |----->---
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T>
inline constexpr T ForwardDiff(T module, T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return (module == 0 || a <= b) ? b - a : module - (a - b);
}

template <typename T, T M = 0>
inline constexpr T ForwardDiff(T a, T b) {
  return ForwardDiff(M, a, b);
}

// Calculates the reverse difference between two wrapping numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ReverseDiff(y, x) == 5
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          <-----<-----<-----<-----<-----|
//
// ReverseDiff(x, y) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// ---<-----|                             |<-----<--
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T>
inline constexpr T ReverseDiff(T module, T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return (module == 0 || b <= a) ? a - b : module - (b - a);
}

template <typename T, T M = 0>
inline constexpr T ReverseDiff(T a, T b) {
  return ReverseDiff(M, a, b);
}

// Calculates the minimum distance between to wrapping numbers.
//
// The minimum distance is defined as min(ForwardDiff(a, b), ReverseDiff(a, b))
template <typename T>
inline constexpr T MinDiff(T module, T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return std::min(ForwardDiff(module, a, b), ReverseDiff(module, a, b));
}

template <typename T, T M = 0>
inline constexpr T MinDiff(T a, T b) {
  return MinDiff(M, a, b);
}

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_MOD_OPS_H_
