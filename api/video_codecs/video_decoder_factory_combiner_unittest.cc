/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_factory_combiner.h"

#include <utility>

#include "api/test/mock_video_decoder.h"
#include "api/video_codecs/video_decoder_factory_item.h"
// #include "api/video_codecs/video_decoder_factory_dav1d.h"
#include "api/video_codecs/video_decoder_factory_libvpx_vp8.h"
#include "api/video_codecs/video_decoder_factory_libvpx_vp9.h"
// #include "api/video_codecs/video_decoder_factory_open_h264.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

SdpVideoFormat FooSdp() {
  return SdpVideoFormat("Foo");
}
SdpVideoFormat BarLowSdp() {
  return SdpVideoFormat("Bar", {{"profile", "low"}});
}
SdpVideoFormat BarHighSdp() {
  return SdpVideoFormat("Bar", {{"profile", "high"}});
}

std::vector<VideoDecoderFactoryItem> FooDecoders() {
  auto factory = [](const FieldTrialsView& experiments) {
    auto decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
    EXPECT_CALL(*decoder, Destruct);
    return decoder;
  };
  std::vector<VideoDecoderFactoryItem> decoders;
  decoders.emplace_back(FooSdp(), std::move(factory));
  return decoders;
}

std::vector<VideoDecoderFactoryItem> BarDecoders() {
  auto factory = [](const FieldTrialsView& experiments) {
    auto decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
    EXPECT_CALL(*decoder, Destruct);
    return decoder;
  };
  std::vector<VideoDecoderFactoryItem> decoders;
  decoders.emplace_back(BarLowSdp(), factory);
  decoders.emplace_back(BarHighSdp(), factory);
  return decoders;
}

TEST(VideoDecoderFactoryCombiner, OneTemplateAdapterCreateDecoder) {
  VideoDecoderFactoryCombiner factory({FooDecoders()});
  EXPECT_THAT(factory.GetSupportedFormats(), UnorderedElementsAre(FooSdp()));
  EXPECT_THAT(factory.CreateVideoDecoder(FooSdp()), NotNull());
  EXPECT_THAT(factory.CreateVideoDecoder(SdpVideoFormat("FooX")), IsNull());
}

TEST(VideoDecoderFactoryCombiner, TwoTemplateAdaptersNoDuplicates) {
  VideoDecoderFactoryCombiner factory({FooDecoders(), FooDecoders()});
  EXPECT_THAT(factory.GetSupportedFormats(), UnorderedElementsAre(FooSdp()));
}

TEST(VideoDecoderFactoryCombiner, TwoTemplateAdaptersCreateDecoders) {
  VideoDecoderFactoryCombiner factory({FooDecoders(), BarDecoders()});

  EXPECT_THAT(factory.GetSupportedFormats(),
              UnorderedElementsAre(FooSdp(), BarLowSdp(), BarHighSdp()));
  EXPECT_THAT(factory.CreateVideoDecoder(FooSdp()), NotNull());
  EXPECT_THAT(factory.CreateVideoDecoder(BarLowSdp()), NotNull());
  EXPECT_THAT(factory.CreateVideoDecoder(BarHighSdp()), NotNull());
  EXPECT_THAT(factory.CreateVideoDecoder(SdpVideoFormat("FooX")), IsNull());
  EXPECT_THAT(factory.CreateVideoDecoder(SdpVideoFormat("Bar")), IsNull());
}

TEST(VideoDecoderFactoryCombiner, LibvpxVp8) {
  VideoDecoderFactoryCombiner factory({LibvpxVp8Decoders()});
  auto formats = factory.GetSupportedFormats();
  ASSERT_THAT(formats, SizeIs(1));
  EXPECT_THAT(formats[0], Field(&SdpVideoFormat::name, "VP8"));
  EXPECT_THAT(factory.CreateVideoDecoder(formats[0]), NotNull());
}

TEST(VideoDecoderFactoryCombiner, LibvpxVp9) {
  VideoDecoderFactoryCombiner factory({LibvpxVp9Decoders()});
  auto formats = factory.GetSupportedFormats();
  EXPECT_THAT(formats, Not(IsEmpty()));
  EXPECT_THAT(formats, Each(Field(&SdpVideoFormat::name, "VP9")));
  EXPECT_THAT(factory.CreateVideoDecoder(formats[0]), NotNull());
}
#if 0
// TODO(bugs.webrtc.org/13573): When OpenH264 is no longer a conditional build
//                              target remove this #ifdef.
#if defined(WEBRTC_USE_H264)
TEST(VideoDecoderFactoryTemplate, OpenH264) {
  VideoDecoderFactoryTemplate<OpenH264DecoderTemplateAdapter> factory;
  auto formats = factory.GetSupportedFormats();
  EXPECT_THAT(formats, Not(IsEmpty()));
  EXPECT_THAT(formats, Each(Field(&SdpVideoFormat::name, "H264")));
  EXPECT_THAT(factory.CreateVideoDecoder(formats[0]), Ne(nullptr));
}
#endif  // defined(WEBRTC_USE_H264)

TEST(VideoDecoderFactoryTemplate, Dav1d) {
  VideoDecoderFactoryTemplate<Dav1dDecoderTemplateAdapter> factory;
  auto formats = factory.GetSupportedFormats();
  EXPECT_THAT(formats, Not(IsEmpty()));
  EXPECT_THAT(formats, Each(Field(&SdpVideoFormat::name, "AV1")));
  EXPECT_THAT(factory.CreateVideoDecoder(formats[0]), Ne(nullptr));
}
#endif
}  // namespace
}  // namespace webrtc
