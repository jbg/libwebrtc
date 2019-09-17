/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/copy_on_write_buffer_view.h"
namespace rtc {

// static
CopyOnWriteBufferView CopyOnWriteBufferView::GetSlice(
    const CopyOnWriteBuffer& buf,
    size_t offset,
    size_t length) {
  RTC_DCHECK_LE(offset + length, buf.size());
  CopyOnWriteBufferView slice(buf);
  slice.offset_ = offset;
  slice.length_ = length;
  return slice;
}

void CopyOnWriteBufferView::CopyIfSliced() {
  RTC_DCHECK(IsConsistent());
  if (IsSliced()) {
    buffer_ = CopyOnWriteBuffer(buffer_.cdata() + offset_, length_);
    offset_ = 0;
  }
  RTC_DCHECK(IsConsistent());
}

CopyOnWriteBufferView CopyOnWriteBufferView::GetSlice(size_t offset,
                                                      size_t length) const {
  RTC_DCHECK_LE(offset + length, length_);
  return GetSlice(buffer_, offset_ + offset, length);
}

CopyOnWriteBuffer CopyOnWriteBufferView::ToBuffer() {
  CopyIfSliced();
  return buffer_;
}

void CopyOnWriteBufferView::Clear() {
  buffer_.Clear();
  offset_ = 0;
  length_ = 0;
}

void CopyOnWriteBufferView::SetSize(size_t new_size) {
  if (new_size > length_) {
    CopyIfSliced();
    buffer_.SetSize(new_size);
  }
  length_ = new_size;
}

}  // namespace rtc
