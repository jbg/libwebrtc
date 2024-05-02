/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_VIEW_H_
#define API_AUDIO_AUDIO_VIEW_H_

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

// TODO(tommi): Add:
//  class MonoView<>. Basically ArrayView<>. size() is samples_per_channel()
//  num_channels() is 1 (could be constexpr for MonoView?).
//  Could MonoView<> be an alias ('using') for ArrayView? For potential
//  direct compatibility with ArrayView? If so, then we'd probably need:
//  template<typename V>
//  size_t NumChannels(const V& v);
//  and a specialisation for MonoView/ArrayView that returns 1, and
//  size_t SamplesPerChannel(const V& v);
//  and a specialisation for MonoView/ArrayView that returns size().
//  For Interlaved and Deinterleaved classes, the specialization would call
//  v.num_channels() and v.samples_per_channel().
//
//  Perhaps add a utility method, IsMono(view) to be used in DCHECKs?
//  RTC_DCHECK(IsMono(view));
//  template<typename V>
//  bool IsMono(const V& v) { return NumChannels(v) == 1u; }
//
// Add InterleavedView<T>. Contains ArrayView<T>. Adds  samples_per_channel(),
// num_channels().
// Supports AsMonoView() with RTC_DCHECK(IsMono(*this)).
//
// Add DeinterleavedView<T>. Contains ArrayView<T>. Adds  samples_per_channel(),
// num_channels().
// Also adds MonoView<T> channel(size_t ch);

// MonoView represents a view over a single contiguous, audio buffer. This
// can be either an single channel (mono) interleaved buffer (e.g. AudioFrame),
// or a de-interleaved channel (e.g. from AudioBuffer).
template <typename T>
using MonoView = rtc::ArrayView<T>;

template <typename T>
class InterleavedView {
 public:
  using value_type = T;

  InterleavedView() = default;

  // TODO(tommi): Dcheck valid ranges of values for `num_channels` and
  // `samples_per_channel`.
  template <typename U>
  InterleavedView(U* data, size_t samples_per_channel, size_t num_channels)
      : num_channels_(num_channels),
        samples_per_channel_(samples_per_channel),
        data_(data, num_channels * samples_per_channel) {}

  // Construct an InterleavedView from a C-style array. Samples per channels
  // is calculated based on the array size / num_channels.
  template <typename U, size_t N>
  InterleavedView(U (&array)[N],  // NOLINT
                  size_t num_channels)
      : InterleavedView(array, N / num_channels, num_channels) {
    RTC_DCHECK_EQ(N % num_channels, 0u);
  }

  template <typename U>
  InterleavedView(const InterleavedView<U>& other)
      : num_channels_(other.num_channels()),
        samples_per_channel_(other.samples_per_channel()),
        data_(other.data()) {}

  size_t num_channels() const { return num_channels_; }
  size_t samples_per_channel() const { return samples_per_channel_; }
  rtc::ArrayView<T> data() const { return data_; }
  bool empty() const { return data_.empty(); }
  size_t size() const { return data_.size(); }

  MonoView<T> AsMono() const {
    RTC_DCHECK_EQ(num_channels(), 1u);
    RTC_DCHECK_EQ(data_.size(), samples_per_channel_);
    return data_;
  }

  // A simple wrapper around memcpy that includes checks for properties.
  // TODO(tommi): Consider if this can be utility function for both interleaved
  // and deinterleaved views.
  template <typename U>
  void CopyFrom(const InterleavedView<U>& source) {
    static_assert(sizeof(T) == sizeof(U), "");
    RTC_DCHECK_EQ(num_channels(), source.num_channels());
    RTC_DCHECK_EQ(samples_per_channel(), source.samples_per_channel());
    RTC_DCHECK_GE(data_.size(), source.data().size());
    const auto data = source.data();
    memcpy(&data_[0], &data[0], data.size() * sizeof(U));
  }

  T& operator[](size_t idx) const { return data_[idx]; }
  T* begin() const { return data_.begin(); }
  T* end() const { return data_.end(); }
  const T* cbegin() const { return data_.cbegin(); }
  const T* cend() const { return data_.cend(); }
  std::reverse_iterator<T*> rbegin() const { return data_.rbegin(); }
  std::reverse_iterator<T*> rend() const { return data_.rend(); }
  std::reverse_iterator<const T*> crbegin() const { return data_.crbegin(); }
  std::reverse_iterator<const T*> crend() const { return data_.crend(); }

 private:
  // TODO(tommi): Consider having these both be stored as uint16_t to
  // save a few bytes per view. Use `dchecked_cast` to support size_t during
  // construction.
  size_t num_channels_ = 0u;
  size_t samples_per_channel_ = 0u;
  rtc::ArrayView<T> data_;
};

template <typename T>
class DeinterleavedView {
 public:
  using value_type = T;

  DeinterleavedView() = default;

  // TODO(tommi): Dcheck valid ranges of values for `num_channels` and
  // `samples_per_channel`.
  // A default value of 0u for `stride` means to assume samples_per_channel as
  // the stride (offset between deinterleaved channels in the buffer).
  template <typename U>
  DeinterleavedView(U* data,
                    size_t samples_per_channel,
                    size_t num_channels,
                    size_t stride = 0u)
      : stride_(stride ? stride : samples_per_channel),
        num_channels_(num_channels),
        samples_per_channel_(samples_per_channel),
        data_(data, num_channels * stride_) {
    RTC_DCHECK_GE(stride_, samples_per_channel_);
    RTC_DCHECK_GE(data_.size(), stride_ * num_channels_);
  }

  template <typename U>
  DeinterleavedView(const DeinterleavedView<U>& other)
      : stride_(other.stride()),
        num_channels_(other.num_channels()),
        samples_per_channel_(other.samples_per_channel()),
        data_(other.data()) {}

  // Returns a deinterleaved channel where `idx` is the zero based index,
  // in the range [0 .. num_channels()-1].
  MonoView<T> operator[](size_t idx) const {
    RTC_DCHECK_LT(idx, num_channels_);
    return MonoView<T>(&data_[idx * stride_], samples_per_channel_);
  }

  size_t stride() const { return stride_; }
  size_t num_channels() const { return num_channels_; }
  size_t samples_per_channel() const { return samples_per_channel_; }
  rtc::ArrayView<T> data() const { return data_; }
  bool empty() const { return data_.empty(); }
  size_t size() const { return data_.size(); }

  // Returns the first (and possibly only) channel.
  MonoView<T> AsMono() const {
    RTC_DCHECK_GE(num_channels(), 1u);
    return (*this)[0];
  }

 private:
  // TODO(tommi): Consider having these be stored as uint16_t to save a few
  // bytes per view. Use `dchecked_cast` to support size_t during construction.
  size_t stride_ = 0u;
  size_t num_channels_ = 0u;
  size_t samples_per_channel_ = 0u;
  rtc::ArrayView<T> data_;
};

template <typename T>
constexpr size_t NumChannels(const MonoView<T>& view) {
  return 1u;
}

template <typename T>
size_t NumChannels(const InterleavedView<T>& view) {
  return view.num_channels();
}

template <typename T>
size_t NumChannels(const DeinterleavedView<T>& view) {
  return view.num_channels();
}

template <typename T>
constexpr bool IsMono(const MonoView<T>& view) {
  return true;
}

template <typename T>
constexpr bool IsInterleavedView(const MonoView<T>& view) {
  return true;
}

template <typename T>
constexpr bool IsInterleavedView(const InterleavedView<T>& view) {
  return true;
}

template <typename T>
constexpr bool IsInterleavedView(const DeinterleavedView<const T>& view) {
  return false;
}

template <typename T>
bool IsMono(const InterleavedView<T>& view) {
  return NumChannels(view) == 1u;
}

template <typename T>
bool IsMono(const DeinterleavedView<T>& view) {
  return NumChannels(view) == 1u;
}

template <typename T>
size_t SamplesPerChannel(const MonoView<T>& view) {
  return view.size();
}

template <typename T>
size_t SamplesPerChannel(const InterleavedView<T>& view) {
  return view.samples_per_channel();
}

template <typename T>
size_t SamplesPerChannel(const DeinterleavedView<T>& view) {
  return view.samples_per_channel();
}
// A simple wrapper around memcpy that includes checks for properties.
// The parameter order is the same as for memcpy(), first destination then
// source.
template <typename D, typename S>
void CopyView(D& destination, const S& source) {
  static_assert(
      sizeof(typename D::value_type) == sizeof(typename S::value_type), "");
  // Here we'd really like to do
  // static_assert(IsInterleavedView(destination) == IsInterleavedView(source),
  //               "");
  // but the compiler doesn't like it inside this template function for
  // some reason. The following check is an approximation but unfortunately
  // means that copying between a MonoView and single channel interleaved or
  // deinterleaved views wouldn't work.
  // static_assert(sizeof(destination) == sizeof(source),
  //               "Incompatible view types");
  RTC_DCHECK_EQ(NumChannels(destination), NumChannels(source));
  RTC_DCHECK_EQ(SamplesPerChannel(destination), SamplesPerChannel(source));
  RTC_DCHECK_GE(destination.data().size(), source.data().size());
  memcpy(&destination[0], &source[0],
         source.size() * sizeof(typename S::value_type));
}

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_VIEW_H_
