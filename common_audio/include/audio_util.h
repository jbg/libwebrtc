/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
#define COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

typedef std::numeric_limits<int16_t> limits_int16;

// The conversion functions use the following naming convention:
// S16:      int16_t [-32768, 32767]
// Float:    float   [-1.0, 1.0]
// FloatS16: float   [-32768.0, 32768.0]
// Dbfs: float [-20.0*log(10, 32768), 0] = [-90.3, 0]
// The ratio conversion functions use this naming convention:
// Ratio: float (0, +inf)
// Db: float (-inf, +inf)
static inline float S16ToFloat(int16_t v) {
  constexpr float kScaling = 1.f / 32768.f;
  return v * kScaling;
}

static inline int16_t FloatS16ToS16(float v) {
  v = std::min(v, 32767.f);
  v = std::max(v, -32768.f);
  return static_cast<int16_t>(v + std::copysign(0.5f, v));
}

static inline int16_t FloatToS16(float v) {
  v *= 32768.f;
  v = std::min(v, 32767.f);
  v = std::max(v, -32768.f);
  return static_cast<int16_t>(v + std::copysign(0.5f, v));
}

static inline float FloatToFloatS16(float v) {
  v = std::min(v, 1.f);
  v = std::max(v, -1.f);
  return v * 32768.f;
}

static inline float FloatS16ToFloat(float v) {
  v = std::min(v, 32768.f);
  v = std::max(v, -32768.f);
  constexpr float kScaling = 1.f / 32768.f;
  return v * kScaling;
}

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
  InterleavedView() = default;

  // TODO(tommi): Dcheck valid ranges of values for `num_channels` and
  // `samples_per_channel`.
  template <typename U>
  InterleavedView(U* data, size_t num_channels, size_t samples_per_channel)
      : num_channels_(num_channels),
        samples_per_channel_(samples_per_channel),
        data_(data, num_channels * samples_per_channel) {}

  // Construct an InterleavedView from a C-style array. Samples per channels
  // is calculated based on the array size / num_channels.
  template <typename U, size_t N>
  InterleavedView(U (&array)[N],  // NOLINT
                  size_t num_channels)
      : InterleavedView(array, num_channels, N / num_channels) {
    RTC_DCHECK_EQ(N % num_channels, 0u);
  }

  template <typename U>
  InterleavedView(const InterleavedView<U>& other)
      : num_channels_(other.num_channels()),
        samples_per_channel_(other.samples_per_channel()),
        data_(other.data()) {}

  template <typename U>
  InterleavedView<T>& operator=(const InterleavedView<U>& other) {
    data_ = other.data_;
    num_channels_ = other.num_channels_;
    samples_per_channel_ = other.samples_per_channel_;
    return *this;
  }

  size_t num_channels() const { return num_channels_; }
  size_t samples_per_channel() const { return samples_per_channel_; }
  rtc::ArrayView<T> data() const { return data_; }

  MonoView<T> AsMono() const {
    RTC_DCHECK_EQ(num_channels(), 1u);
    RTC_DCHECK_EQ(data_.size(), samples_per_channel_);
    return data_;
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
  DeinterleavedView() = default;

  // TODO(tommi): Dcheck valid ranges of values for `num_channels` and
  // `samples_per_channel`.
  // A default value of 0u for `stride` means to assume samples_per_channel as
  // the stride (offset between deinterleaved channels in the buffer).
  template <typename U>
  DeinterleavedView(U* data,
                    size_t num_channels,
                    size_t samples_per_channel,
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
      : stride_(other.stride_),
        num_channels_(other.num_channels()),
        samples_per_channel_(other.samples_per_channel()),
        data_(other.data()) {}

  template <typename U>
  DeinterleavedView<T>& operator=(const DeinterleavedView<U>& other) {
    stride_ = other.stride_;
    data_ = other.data_;
    num_channels_ = other.num_channels_;
    samples_per_channel_ = other.samples_per_channel_;
    return *this;
  }

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

void FloatToS16(const float* src, size_t size, int16_t* dest);
void S16ToFloat(const int16_t* src, size_t size, float* dest);
void S16ToFloatS16(const int16_t* src, size_t size, float* dest);
void FloatS16ToS16(const float* src, size_t size, int16_t* dest);
void FloatToFloatS16(const float* src, size_t size, float* dest);
void FloatS16ToFloat(const float* src, size_t size, float* dest);

inline float DbToRatio(float v) {
  return std::pow(10.0f, v / 20.0f);
}

inline float DbfsToFloatS16(float v) {
  static constexpr float kMaximumAbsFloatS16 = -limits_int16::min();
  return DbToRatio(v) * kMaximumAbsFloatS16;
}

inline float FloatS16ToDbfs(float v) {
  RTC_DCHECK_GE(v, 0);

  // kMinDbfs is equal to -20.0 * log10(-limits_int16::min())
  static constexpr float kMinDbfs = -90.30899869919436f;
  if (v <= 1.0f) {
    return kMinDbfs;
  }
  // Equal to 20 * log10(v / (-limits_int16::min()))
  return 20.0f * std::log10(v) + kMinDbfs;
}

// Copy audio from `src` channels to `dest` channels unless `src` and `dest`
// point to the same address. `src` and `dest` must have the same number of
// channels, and there must be sufficient space allocated in `dest`.
template <typename T>
void CopyAudioIfNeeded(const T* const* src,
                       int num_frames,
                       int num_channels,
                       T* const* dest) {
  for (int i = 0; i < num_channels; ++i) {
    if (src[i] != dest[i]) {
      std::copy(src[i], src[i] + num_frames, dest[i]);
    }
  }
}

// Deinterleave audio from `interleaved` to the channel buffers pointed to
// by `deinterleaved`. There must be sufficient space allocated in the
// `deinterleaved` buffers (`num_channel` buffers with `samples_per_channel`
// per buffer).
// TODO: b/335805780 - Accept AudioFrame::View<const T> for const T*.
// Figure something out for `deinterleaved` too.
template <typename T>
void Deinterleave(const T* interleaved,
                  size_t samples_per_channel,
                  size_t num_channels,
                  T* const* deinterleaved) {
  for (size_t i = 0; i < num_channels; ++i) {
    T* channel = deinterleaved[i];
    size_t interleaved_idx = i;
    for (size_t j = 0; j < samples_per_channel; ++j) {
      channel[j] = interleaved[interleaved_idx];
      interleaved_idx += num_channels;
    }
  }
}

// Interleave audio from the channel buffers pointed to by `deinterleaved` to
// `interleaved`. There must be sufficient space allocated in `interleaved`
// (`samples_per_channel` * `num_channels`).
template <typename T>
void Interleave(const T* const* deinterleaved,
                size_t samples_per_channel,
                size_t num_channels,
                T* interleaved) {
  for (size_t i = 0; i < num_channels; ++i) {
    const T* channel = deinterleaved[i];
    size_t interleaved_idx = i;
    for (size_t j = 0; j < samples_per_channel; ++j) {
      interleaved[interleaved_idx] = channel[j];
      interleaved_idx += num_channels;
    }
  }
}

// Copies audio from a single channel buffer pointed to by `mono` to each
// channel of `interleaved`. There must be sufficient space allocated in
// `interleaved` (`samples_per_channel` * `num_channels`).
template <typename T>
void UpmixMonoToInterleaved(const T* mono,
                            int num_frames,
                            int num_channels,
                            T* interleaved) {
  int interleaved_idx = 0;
  for (int i = 0; i < num_frames; ++i) {
    for (int j = 0; j < num_channels; ++j) {
      interleaved[interleaved_idx++] = mono[i];
    }
  }
}

template <typename T, typename Intermediate>
void DownmixToMono(const T* const* input_channels,
                   size_t num_frames,
                   int num_channels,
                   T* out) {
  for (size_t i = 0; i < num_frames; ++i) {
    Intermediate value = input_channels[0][i];
    for (int j = 1; j < num_channels; ++j) {
      value += input_channels[j][i];
    }
    out[i] = value / num_channels;
  }
}

// Downmixes an interleaved multichannel signal to a single channel by averaging
// all channels.
template <typename T, typename Intermediate>
void DownmixInterleavedToMonoImpl(const T* interleaved,
                                  size_t num_frames,
                                  int num_channels,
                                  T* deinterleaved) {
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_GT(num_frames, 0);

  const T* const end = interleaved + num_frames * num_channels;

  while (interleaved < end) {
    const T* const frame_end = interleaved + num_channels;

    Intermediate value = *interleaved++;
    while (interleaved < frame_end) {
      value += *interleaved++;
    }

    *deinterleaved++ = value / num_channels;
  }
}

template <typename T>
void DownmixInterleavedToMono(const T* interleaved,
                              size_t num_frames,
                              int num_channels,
                              T* deinterleaved);

template <>
void DownmixInterleavedToMono<int16_t>(const int16_t* interleaved,
                                       size_t num_frames,
                                       int num_channels,
                                       int16_t* deinterleaved);

}  // namespace webrtc

#endif  // COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
