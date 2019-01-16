/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"

#include "api/array_view.h"
#include "rtc_base/checks.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

// TODO(danilchap): Add fuzzer to test for various invalid inputs.

enum class Version { kVersion00, kVersion01 };

class RtpGenericFrameDescriptorExtensionTest
    : public ::testing::TestWithParam<Version> {
 public:
  RtpGenericFrameDescriptorExtensionTest() : version_(GetParam()) {}
  ~RtpGenericFrameDescriptorExtensionTest() override = default;

  bool Parse(rtc::ArrayView<const uint8_t> data,
             RtpGenericFrameDescriptor* descriptor) {
    switch (version_) {
      case Version::kVersion00:
        return RtpGenericFrameDescriptorExtension00::Parse(data, descriptor);
      case Version::kVersion01:
        return RtpGenericFrameDescriptorExtension01::Parse(data, descriptor);
    }
    RTC_NOTREACHED();
    return false;
  }

  size_t ValueSize(const RtpGenericFrameDescriptor& descriptor) {
    switch (version_) {
      case Version::kVersion00:
        return RtpGenericFrameDescriptorExtension00::ValueSize(descriptor);
      case Version::kVersion01:
        return RtpGenericFrameDescriptorExtension01::ValueSize(descriptor);
    }
    RTC_NOTREACHED();
    return 0;
  }

  bool Write(rtc::ArrayView<uint8_t> data,
             const RtpGenericFrameDescriptor& descriptor) {
    switch (version_) {
      case Version::kVersion00:
        return RtpGenericFrameDescriptorExtension00::Write(data, descriptor);
      case Version::kVersion01:
        return RtpGenericFrameDescriptorExtension01::Write(data, descriptor);
    }
    RTC_NOTREACHED();
    return false;
  }

  uint8_t MaybeAddSubFrameFlags(uint8_t input) const {
    switch (version_) {
      case Version::kVersion00:
        return input | 0x30;
      case Version::kVersion01:
        return input | 0x00;
    }
    RTC_NOTREACHED();
    return input | 0x00;
  }

  const Version version_;
};

INSTANTIATE_TEST_CASE_P(_,
                        RtpGenericFrameDescriptorExtensionTest,
                        ::testing::Values(Version::kVersion00,
                                          Version::kVersion01));

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseFirstPacketOfIndependenFrame) {
  const int kTemporalLayer = 5;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x80 | kTemporalLayer), 0x49,
                          0x12, 0x34};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_FALSE(descriptor.LastPacketInFrame());
  // TODO(eladalon): Parse decodability flag when it's introduced.
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), IsEmpty());
  EXPECT_EQ(descriptor.TemporalLayer(), kTemporalLayer);
  EXPECT_EQ(descriptor.SpatialLayersBitmask(), 0x49);
  EXPECT_EQ(descriptor.FrameId(), 0x3412);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteFirstPacketOfIndependenFrame) {
  const int kTemporalLayer = 5;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x80 | kTemporalLayer), 0x49,
                          0x12, 0x34};
  RtpGenericFrameDescriptor descriptor;

  descriptor.SetFirstPacketInFrame(true);
  descriptor.SetTemporalLayer(kTemporalLayer);
  descriptor.SetSpatialLayersBitmask(0x49);
  descriptor.SetFrameId(0x3412);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastPacketOfFrame) {
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x40)};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInFrame());
  EXPECT_TRUE(descriptor.LastPacketInFrame());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteLastPacketOfFrame) {
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x40)};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetLastPacketInFrame(true);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x04};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x04};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0xfc};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0xfc};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x02, 0x01};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x02, 0x01};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0xfe, 0xff};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x88), 0x01, 0x00, 0x00, 0xfe, 0xff};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01,       0x00, 0x00,
                          (kDiff1 << 2) | 0x01,        kDiff2 << 2};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff1, kDiff2));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;
  const uint8_t kRaw[] = {MaybeAddSubFrameFlags(0x88), 0x01,       0x00, 0x00,
                          (kDiff1 << 2) | 0x01,        kDiff2 << 2};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff1);
  descriptor.AddFrameDependencyDiff(kDiff2);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x80), 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  EXPECT_EQ(descriptor.Width(), kWidth);
  EXPECT_EQ(descriptor.Height(), kHeight);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;
  const uint8_t kRaw[] = {
      MaybeAddSubFrameFlags(0x80), 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInFrame(true);
  descriptor.SetResolution(kWidth, kHeight);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}
}  // namespace
}  // namespace webrtc
