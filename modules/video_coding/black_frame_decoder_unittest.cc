/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/black_frame_decoder.h"

#include <memory>

#include "api/test/mock_video_decoder.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace webrtc {

class BlackFrameDecoderTest : public ::testing::Test {
 public:
  BlackFrameDecoderTest()
      : decoder_(
            std::make_unique<BlackFrameDecoder>(SdpVideoFormat("TestFormat"))) {
  }
  ~BlackFrameDecoderTest() override = default;

 protected:
  std::unique_ptr<BlackFrameDecoder> decoder_;
};

TEST_F(BlackFrameDecoderTest, DecodesFrameWithSize) {
  EncodedImage image;
  image._frameType = VideoFrameType::kVideoFrameKey;
  image._encodedWidth = 314;
  image._encodedHeight = 62;

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder_->RegisterDecodeCompleteCallback(&decoder_callback), 0);
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 314);
        EXPECT_EQ(decodedImage.height(), 62);
      }));
  EXPECT_EQ(0, decoder_->Decode(image, false, 0));

  image._frameType = VideoFrameType::kVideoFrameDelta;
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 314);
        EXPECT_EQ(decodedImage.height(), 62);
      }));
  EXPECT_EQ(0, decoder_->Decode(image, false, 0));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(BlackFrameDecoderTest, DiesIfEncodedSizeNotSet) {
  // BlackFrameDecoder decoder(SdpVideoFormat("TestFormat"));
  EncodedImage image;
  image._frameType = VideoFrameType::kVideoFrameKey;
  image._encodedWidth = 0;
  image._encodedHeight = 1;
  EXPECT_DEATH(decoder_->Decode(image, false, 0), "");
  image._encodedWidth = 1;
  image._encodedWidth = 0;
  EXPECT_DEATH(decoder_->Decode(image, false, 0), "");
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
