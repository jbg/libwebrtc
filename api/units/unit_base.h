/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_UNITS_UNIT_BASE_H_
#define API_UNITS_UNIT_BASE_H_

#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace rtc_units_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();

template <class Unit_T>
class UnitBase {
 public:
  UnitBase() = delete;
  static constexpr Unit_T Zero() { return Unit_T(0); }
  static constexpr Unit_T PlusInfinity() {
    return Unit_T(rtc_units_impl::kPlusInfinityVal);
  }
  static constexpr Unit_T MinusInfinity() {
    return Unit_T(rtc_units_impl::kMinusInfinityVal);
  }

  constexpr bool IsZero() const { return value_ == 0; }
  constexpr bool IsFinite() const { return !IsInfinite(); }
  constexpr bool IsInfinite() const {
    return value_ == kPlusInfinityVal || value_ == kMinusInfinityVal;
  }
  constexpr bool IsPlusInfinity() const { return value_ == kPlusInfinityVal; }
  constexpr bool IsMinusInfinity() const { return value_ == kMinusInfinityVal; }

  constexpr bool operator==(const Unit_T& other) const {
    return value_ == other.value_;
  }
  constexpr bool operator!=(const Unit_T& other) const {
    return value_ != other.value_;
  }
  constexpr bool operator<=(const Unit_T& other) const {
    return value_ <= other.value_;
  }
  constexpr bool operator>=(const Unit_T& other) const {
    return value_ >= other.value_;
  }
  constexpr bool operator>(const Unit_T& other) const {
    return value_ > other.value_;
  }
  constexpr bool operator<(const Unit_T& other) const {
    return value_ < other.value_;
  }

 protected:
  template <int64_t value>
  static constexpr Unit_T FromStaticValue() {
    static_assert(value >= 0 || !Unit_T::one_sided, "");
    static_assert(value > kMinusInfinityVal, "");
    static_assert(value < kPlusInfinityVal, "");
    return Unit_T(value);
  }

  template <int64_t fraction_value, int64_t Denominator>
  static constexpr Unit_T FromStaticFraction() {
    static_assert(fraction_value >= 0 || !Unit_T::one_sided, "");
    static_assert(fraction_value > kMinusInfinityVal / Denominator, "");
    static_assert(fraction_value < kPlusInfinityVal / Denominator, "");
    return Unit_T(fraction_value * Denominator);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static Unit_T FromValue(T value) {
    if (Unit_T::one_sided)
      RTC_DCHECK_GE(value, 0);
    RTC_DCHECK_GT(value, kMinusInfinityVal);
    RTC_DCHECK_LT(value, kPlusInfinityVal);
    return Unit_T(rtc::dchecked_cast<int64_t>(value));
  }
  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static Unit_T FromValue(T value) {
    if (value == std::numeric_limits<T>::infinity()) {
      return PlusInfinity();
    } else if (value == -std::numeric_limits<T>::infinity()) {
      return MinusInfinity();
    } else {
      RTC_DCHECK(!std::isnan(value));
      return FromValue(rtc::dchecked_cast<int64_t>(value));
    }
  }

  template <
      int64_t Denominator,
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static Unit_T FromFraction(T value) {
    RTC_DCHECK_GT(value,
                  Unit_T::one_sided ? 0 : kMinusInfinityVal / Denominator);
    RTC_DCHECK_LT(value, kPlusInfinityVal / Denominator);
    return Unit_T(rtc::dchecked_cast<int64_t>(value * Denominator));
  }
  template <int64_t Denominator,
            typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static Unit_T FromFraction(T value) {
    return FromValue(value * Denominator);
  }

  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type ToValue() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(value_);
  }
  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  ToValue() const {
    return IsPlusInfinity()
               ? std::numeric_limits<T>::infinity()
               : IsMinusInfinity() ? -std::numeric_limits<T>::infinity()
                                   : value_;
  }
  template <typename T>
  constexpr T ToValueOr(T fallback_value) const {
    return IsFinite() ? value_ : fallback_value;
  }

  template <int64_t Denominator, typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type ToFraction()
      const {
    RTC_DCHECK(IsFinite());
    if (Unit_T::one_sided) {
      return rtc::dchecked_cast<T>(UnsafeFractionOneSided<Denominator>());
    } else {
      return rtc::dchecked_cast<T>(UnsafeFractionTwoSided<Denominator>());
    }
  }
  template <int64_t Denominator, typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  ToFraction() const {
    return ToValue<T>() * (1 / static_cast<T>(Denominator));
  }

  template <int64_t Denominator>
  constexpr int64_t ToFractionOr(int64_t fallback_value) const {
    return IsFinite() ? Unit_T::one_sided
                            ? UnsafeFractionOneSided<Denominator>()
                            : UnsafeFractionTwoSided<Denominator>()
                      : fallback_value;
  }

  template <int64_t Factor, typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type ToMultiple()
      const {
    RTC_DCHECK_GE(ToValue(), std::numeric_limits<T>::min() / Factor);
    RTC_DCHECK_LE(ToValue(), std::numeric_limits<T>::max() / Factor);
    return rtc::dchecked_cast<T>(ToValue() * Factor);
  }
  template <int64_t Factor, typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  ToMultiple() const {
    return ToValue<T>() * Factor;
  }

  explicit constexpr UnitBase(int64_t value) : value_(value) {}

 private:
  template <class RelativeUnit_T>
  friend class RelativeUnit;
  Unit_T& AsSubClassRef() { return reinterpret_cast<Unit_T&>(*this); }
  constexpr const Unit_T& AsSubClassRef() const {
    return reinterpret_cast<const Unit_T&>(*this);
  }
  template <int64_t Denominator>
  constexpr int64_t UnsafeFractionOneSided() const {
    return (value_ + Denominator / 2) / Denominator;
  }
  template <int64_t Denominator>
  constexpr int64_t UnsafeFractionTwoSided() const {
    return (value_ + (value_ >= 0 ? Denominator / 2 : -Denominator / 2)) /
           Denominator;
  }
  int64_t value_;
};

template <class Unit_T>
class RelativeUnit : public UnitBase<Unit_T> {
 public:
  Unit_T Clamped(Unit_T min_value, Unit_T max_value) const {
    return std::max(min_value,
                    std::min(UnitBase<Unit_T>::AsSubClassRef(), max_value));
  }
  void Clamp(Unit_T min_value, Unit_T max_value) {
    *this = Clamped(min_value, max_value);
  }
  Unit_T operator+(const Unit_T& other) const {
    if (this->IsPlusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!this->IsMinusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return this->PlusInfinity();
    } else if (this->IsMinusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!this->IsPlusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return this->MinusInfinity();
    }
    return UnitBase<Unit_T>::FromValue(this->ToValue() + other.ToValue());
  }
  Unit_T operator-(const Unit_T& other) const {
    if (this->IsPlusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!this->IsMinusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return this->PlusInfinity();
    } else if (this->IsMinusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!this->IsPlusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return this->MinusInfinity();
    }
    return UnitBase<Unit_T>::FromValue(this->ToValue() - other.ToValue());
  }
  Unit_T& operator+=(const Unit_T& other) {
    *this = *this + other;
    return this->AsSubClassRef();
  }
  Unit_T& operator-=(const Unit_T& other) {
    *this = *this - other;
    return this->AsSubClassRef();
  }
  constexpr double operator/(const Unit_T& other) const {
    return UnitBase<Unit_T>::template ToValue<double>() /
           other.template ToValue<double>();
  }
  template <typename T>
  typename std::enable_if<std::is_arithmetic<T>::value, Unit_T>::type operator/(
      const T& scalar) const {
    return UnitBase<Unit_T>::FromValue(
        std::round(UnitBase<Unit_T>::template ToValue<int64_t>() / scalar));
  }
  Unit_T operator*(const double& scalar) const {
    return UnitBase<Unit_T>::FromValue(std::round(this->ToValue() * scalar));
  }
  Unit_T operator*(const int64_t& scalar) const {
    return UnitBase<Unit_T>::FromValue(this->ToValue() * scalar);
  }
  Unit_T operator*(const int32_t& scalar) const {
    return UnitBase<Unit_T>::FromValue(this->ToValue() * scalar);
  }

 protected:
  using UnitBase<Unit_T>::UnitBase;
};

template <class Unit_T>
inline Unit_T operator*(const double& scalar,
                        const RelativeUnit<Unit_T>& other) {
  return other * scalar;
}
template <class Unit_T>
inline Unit_T operator*(const int64_t& scalar,
                        const RelativeUnit<Unit_T>& other) {
  return other * scalar;
}
template <class Unit_T>
inline Unit_T operator*(const int32_t& scalar,
                        const RelativeUnit<Unit_T>& other) {
  return other * scalar;
}

}  // namespace rtc_units_impl

}  // namespace webrtc

#endif  // API_UNITS_UNIT_BASE_H_
