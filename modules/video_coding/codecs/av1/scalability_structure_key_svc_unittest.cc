/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_key_svc.h"

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/scalability_structure_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(ScalabilityStructureL3T3KeyTest,
     SkipingT1FrameOnOneSpatialLayerKeepsStructureValid) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  auto frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 0);

  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 2);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/1));
  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(1));
  EXPECT_EQ(frames[0].temporal_id, 1);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  // Rely on checks inside GenerateFrames frame references are valid.
  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 2);
}

TEST(ScalabilityStructureL3T3KeyTest,
     ReenablingSpatialLayerBeforeMissedT0FrameDoesntTriggerAKeyFrame) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2));
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(2));  // temporal_id == 0
  // Drop a spatial layer.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0));
  auto frames = wrapper.GenerateFrames(1);
  ASSERT_THAT(frames, SizeIs(1));
  EXPECT_EQ(frames[0].temporal_id, 1);
  // Reenable a spatial layer before T0 frame is encoded.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2));
  frames = wrapper.GenerateFrames(1);
  ASSERT_THAT(frames, SizeIs(2));
  // Expect delta frames.
  EXPECT_THAT(frames[0].frame_diffs, SizeIs(1));
  EXPECT_THAT(frames[1].frame_diffs, SizeIs(1));
}

TEST(ScalabilityStructureL3T3KeyTest, ReenablingSpatialLayerTriggersKeyFrame) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);

  // Start with all spatial layers enabled.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2, /*s2=*/2));
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(3));  // temporal_id == 0
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(3));  // temporal_id == 1
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(3));  // temporal_id == 0
  // Drop a spatial layer. Two remaining spatial layers should just continue.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0, /*s2=*/2));
  auto frames = wrapper.GenerateFrames(1);
  ASSERT_THAT(frames, SizeIs(2));
  EXPECT_THAT(frames[0].frame_diffs, SizeIs(1));
  EXPECT_THAT(frames[1].frame_diffs, SizeIs(1));
  EXPECT_EQ(frames[0].spatial_id, 0);
  EXPECT_EQ(frames[1].spatial_id, 2);
  EXPECT_EQ(frames[0].temporal_id, 1);
  EXPECT_EQ(frames[1].temporal_id, 1);
  // Encode a T0 frame while spatial layer 1 is disabled.
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(2));  // temporal_id == 0
  // Reenable spatial layer, expect a full restart.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2, /*s2=*/2));
  frames = wrapper.GenerateFrames(1);
  ASSERT_THAT(frames, SizeIs(3));
  EXPECT_THAT(frames[0].frame_diffs, IsEmpty());
  EXPECT_THAT(frames[1].frame_diffs, ElementsAre(1));
  EXPECT_THAT(frames[2].frame_diffs, ElementsAre(1));
  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 0);
}

}  // namespace
}  // namespace webrtc
