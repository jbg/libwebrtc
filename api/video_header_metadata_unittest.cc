/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_header_metadata.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using testing::ElementsAreArray;
using testing::IsEmpty;

TEST(VideoHeaderMetadata, GetWidthReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  VideoHeaderMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetWidth(), video_header.width);
}

TEST(VideoHeaderMetadata, GetHeightReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.height = 720u;
  VideoHeaderMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetHeight(), video_header.height);
}

TEST(VideoHeaderMetadata, GetFrameIdReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  VideoHeaderMetadata metadata(video_header);
  EXPECT_TRUE(metadata.GetFrameId());
  EXPECT_EQ(metadata.GetFrameId().value(), video_header.generic->frame_id);
}

TEST(VideoHeaderMetadata, HasNoFrameIdForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoHeaderMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_FALSE(metadata.GetFrameId());
}

TEST(VideoHeaderMetadata, GetSpatialIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.spatial_index = 2;
  VideoHeaderMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetSpatialIndex(), video_header.generic->spatial_index);
}

TEST(VideoHeaderMetadata, SpatialIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoHeaderMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetSpatialIndex(), 0);
}

TEST(VideoHeaderMetadata, GetTemporalIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.temporal_index = 3;
  VideoHeaderMetadata metadata(video_header);
  EXPECT_EQ(metadata.GetTemporalIndex(), video_header.generic->temporal_index);
}

TEST(VideoHeaderMetadata, TemporalIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoHeaderMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetTemporalIndex(), 0);
}

TEST(VideoHeaderMetadata, GetFrameDependenciesReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.dependencies = {5, 6, 7};
  VideoHeaderMetadata metadata(video_header);
  EXPECT_THAT(metadata.GetFrameDependencies(),
              ElementsAreArray(video_header.generic->dependencies));
}

TEST(VideoHeaderMetadata, FrameDependencyVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoHeaderMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetFrameDependencies(), IsEmpty());
}

TEST(VideoHeaderMetadata, GetDecodeTargetIndicationsReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  VideoHeaderMetadata metadata(video_header);
  EXPECT_THAT(
      metadata.GetDecodeTargetIndications(),
      ElementsAreArray(video_header.generic->decode_target_indications));
}

TEST(VideoHeaderMetadata,
     DecodeTargetIndicationsVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoHeaderMetadata metadata(video_header);
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(), IsEmpty());
}

}  // namespace
}  // namespace webrtc
