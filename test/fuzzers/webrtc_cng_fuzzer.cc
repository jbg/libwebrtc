/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/array_view.h"
#include "api/optional.h"
#include "modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace test {
namespace {

class DataServer {
 public:
  explicit DataServer(rtc::ArrayView<const uint8_t> data) : data_(data) {}

  rtc::ArrayView<const uint8_t> GetData(size_t bytes) {
    if (bytes_left() < bytes) {
      return rtc::ArrayView<const uint8_t>(nullptr, 0);
    }
    const size_t index_to_return = next_index_;
    next_index_ += bytes;
    return data_.subview(index_to_return, bytes);
  }

  // Return one byte of data. Caller must ensure that there is at least one
  // byte left (using bytes_left()).
  uint8_t GetByte() {
    RTC_DCHECK_GE(bytes_left(), 1);
    return GetData(1)[0];
  }

  size_t bytes_left() const { return data_.size() - next_index_; }

 private:
  rtc::ArrayView<const uint8_t> data_;
  size_t next_index_ = 0;
};

void FuzzOneInputTest(rtc::ArrayView<const uint8_t> data) {
  DataServer data_server(data);
  ComfortNoiseDecoder cng_decoder;

  while (1) {
    if (data_server.bytes_left() < 1)
      break;
    const uint8_t sid_frame_len = data_server.GetByte();
    auto sid_frame = data_server.GetData(sid_frame_len);
    if (sid_frame.empty())
      break;
    cng_decoder.UpdateSid(sid_frame);
    if (data_server.bytes_left() < 3)
      break;
    const bool new_period = data_server.GetByte() % 2;
    auto output_size_generator = [&](uint8_t x) {
      uint8_t ix = x % 4;
      switch (ix) {
        case 0:
          return 80;
        case 1:
          return 160;
        case 2:
          return 320;
        case 3:
          return 480;
        default:
          RTC_NOTREACHED();
          return 0;
      }
    };
    const size_t output_size = output_size_generator(data_server.GetByte());
    const size_t num_generate_calls = data_server.GetByte();
    rtc::BufferT<int16_t> output(output_size);
    for (size_t i = 0; i < num_generate_calls; ++i) {
      cng_decoder.Generate(output, new_period);
    }
  }
}

}  // namespace
}  // namespace test

void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzOneInputTest(rtc::ArrayView<const uint8_t>(data, size));
}

}  // namespace webrtc
