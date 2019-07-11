/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_COPY_ON_WRITE_BUFFER_VIEW_H_
#define RTC_BASE_COPY_ON_WRITE_BUFFER_VIEW_H_

#include <utility>

#include "rtc_base/copy_on_write_buffer.h"

namespace rtc {

class CopyOnWriteBufferView {
 public:
  // An empty buffer.
  CopyOnWriteBufferView() : buffer_(), offset_(0), length_(0) {}
  // Share the data with an existing buffer.
  CopyOnWriteBufferView(const CopyOnWriteBufferView& buf)
      : buffer_(buf.buffer_), offset_(buf.offset_), length_(buf.length_) {}
  explicit CopyOnWriteBufferView(const CopyOnWriteBuffer& buf)
      : buffer_(buf), offset_(0), length_(buf.size()) {}
  // Move contents from an existing buffer.
  CopyOnWriteBufferView(CopyOnWriteBufferView&& buf)
      : buffer_(std::move(buf.buffer_)),
        offset_(buf.offset_),
        length_(buf.length_) {
    buf.offset_ = 0;
    buf.length_ = 0;
  }
  explicit CopyOnWriteBufferView(CopyOnWriteBuffer&& buf)
      : buffer_(std::move(buf)), offset_(0), length_(buf.size()) {}

  static CopyOnWriteBufferView GetSlice(const CopyOnWriteBuffer& buf,
                                        size_t offset,
                                        size_t length);

  ~CopyOnWriteBufferView() = default;

  // Get a pointer to the data. Just .data() will give you a (const) uint8_t*,
  // but you may also use .data<int8_t>() and .data<char>().
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  const T* data() const {
    return cdata<T>();
  }

  // Get writable pointer to the data. This will create a copy of the underlying
  // data if it is shared with other buffers.
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  T* data() {
    RTC_DCHECK(IsConsistent());
    CopyIfSliced();
    RTC_DCHECK_EQ(offset_, 0);
    RTC_DCHECK_EQ(length_, buffer_.size());
    return buffer_.data<T>();
  }

  // Get const pointer to the data. This will not create a copy of the
  // underlying data if it is shared with other buffers.
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  const T* cdata() const {
    RTC_DCHECK(IsConsistent());
    return buffer_.cdata<T>() + offset_;
  }

  size_t size() const {
    RTC_DCHECK(IsConsistent());
    return length_;
  }

  CopyOnWriteBufferView& operator=(const CopyOnWriteBuffer& buf) {
    RTC_DCHECK(IsConsistent());
    buffer_ = buf;
    offset_ = 0;
    length_ = buf.size();
    return *this;
  }

  CopyOnWriteBufferView& operator=(const CopyOnWriteBufferView& buf) {
    RTC_DCHECK(IsConsistent());
    if (&buf != this) {
      buffer_ = buf.buffer_;
      offset_ = buf.offset_;
      length_ = buf.length_;
    }
    return *this;
  }

  CopyOnWriteBufferView& operator=(CopyOnWriteBuffer&& buf) {
    RTC_DCHECK(IsConsistent());
    buffer_ = std::move(buf);
    offset_ = 0;
    length_ = buffer_.size();
    return *this;
  }

  CopyOnWriteBufferView& operator=(CopyOnWriteBufferView&& buf) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK(buf.IsConsistent());
    buffer_ = std::move(buf.buffer_);
    offset_ = buf.offset_;
    length_ = buf.length_;
    buf.offset_ = 0;
    buf.length_ = 0;
    return *this;
  }

  bool operator==(const CopyOnWriteBufferView& buf) const {
    // Some lightweight cases, like the same object, obviously unequal slices,
    // same slices of the same data or empty views.
    if (&buf == this)
      return true;
    if (length_ != buf.length_)
      return false;
    if (length_ == 0)
      return true;
    if (cdata() == buf.cdata())
      return true;
    // General case - compare content.
    return memcmp(cdata(), buf.cdata(), length_) == 0;
  }
  bool operator==(const CopyOnWriteBuffer& buf) const {
    // Some lightweight cases, like  obviously unequal slices,
    // same slices of the same data or empty views.
    if (length_ != buf.size())
      return false;
    if (length_ == 0)
      return true;
    if (cdata() == buf.cdata())
      return true;
    // General case - compare content.
    return memcmp(cdata(), buf.cdata(), length_) == 0;
  }

  bool operator!=(const CopyOnWriteBufferView& buf) const {
    return !(*this == buf);
  }
  bool operator!=(const CopyOnWriteBuffer& buf) const {
    return !(*this == buf);
  }

  uint8_t& operator[](size_t index) {
    RTC_DCHECK_LT(index, size());
    return data()[index];
  }

  uint8_t operator[](size_t index) const {
    RTC_DCHECK_LT(index, size());
    return cdata()[index];
  }

  // Creates a new view. Doesn't copy any data.
  CopyOnWriteBufferView GetSlice(size_t offset, size_t length) const;

  // Converts slice to COW buffer. May copy data if needed.
  CopyOnWriteBuffer ToBuffer();

  // Resets the buffer to zero size without altering capacity. Works even if the
  // buffer has been moved from.
  void Clear();

  void SetSize(size_t new_size);

  // Replace the contents of the buffer. Accepts the same types as the
  // constructors.
  template <typename T,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  void SetData(const T* data, size_t length) {
    buffer_.SetData(data, length);
    offset_ = 0;
    length_ = length;
  }

 private:
  // Pre- and postcondition of all methods.
  bool IsConsistent() const { return offset_ + length_ <= buffer_.size(); }

  bool IsSliced() const { return offset_ != 0 || length_ < buffer_.size(); }

  void CopyIfSliced();

  CopyOnWriteBuffer buffer_;
  size_t offset_;
  size_t length_;
};

}  // namespace rtc

#endif  // RTC_BASE_COPY_ON_WRITE_BUFFER_VIEW_H_
