/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/active_decode_targets_helper.h"

#include "absl/types/optional.h"
#include "test/gtest.h"

namespace webrtc {

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptOnKeyFrameWhenAllDecodeTargetsAreActive) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/{true, true},
                 /*is_keyframe=*/true, /*frame_is_part_of_chain=*/{true});

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsBitmaskOnKeyFrameWhenSomeDecodeTargetsAreInactive) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  const std::vector<bool> kSome = {true, false};
  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/true, /*frame_is_part_of_chain=*/{true});

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptOnDeltaFrameAfterSentOnKeyFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  const std::vector<bool> kSome = {true, false};
  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/true, /*frame_is_part_of_chain=*/{true});
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false, /*frame_is_part_of_chain=*/{false});

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsNewBitmaskOnDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  const std::vector<bool> kAll = {true, true};
  const std::vector<bool> kSome = {true, false};
  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/true, /*frame_is_part_of_chain=*/{true});
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false, /*frame_is_part_of_chain=*/{false});

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsBitmaskWhenAllDecodeTargetsReactivatedOnDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  const std::vector<bool> kAll = {true, true};
  const std::vector<bool> kSome = {true, false};
  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/true, /*frame_is_part_of_chain=*/{true});
  ASSERT_NE(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false, /*frame_is_part_of_chain=*/{false});
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  // Reactive all the decode targets
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/false, /*frame_is_part_of_chain=*/{false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b11u);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsNulloptAfterSentOnAllActiveChains) {
  const std::vector<bool> kAll = {true, true, true};
  // Active decode targets are protected by chains 1 and 2.
  constexpr int kDecodeTargetProtectedByChain[] = {2, 1, 0};
  const std::vector<bool> kSome = {true, true, false};

  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/{true, true, true});
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/{false, false, false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/{false, false, true});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/{false, true, false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  // active_decode_targets_bitmask was send on chains 1 and 2. It was never sent
  // on chain 0, but chain 0 only protects inactive decode target#2
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/{false, false, false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsBitmaskWhenChanged) {
  const std::vector<bool> kAll = {true, true, true};
  constexpr int kDecodeTargetProtectedByChain[] = {0, 1, 1};
  const std::vector<bool> kSome1 = {true, true, false};
  const std::vector<bool> kSome2 = {true, false, true};

  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain, /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/{true, true, true});
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome1,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/{true, false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome2,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/{false, true});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b101u);

  // active_decode_target_bitmask was send on chain0, but it was an old one.
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome2,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/{false, false});
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b101u);
}

TEST(ActiveDecodeTargetsHelperTest, Supports32DecodeTargets) {
  std::vector<bool> all(32, true);
  std::vector<bool> some(32);
  std::vector<int> decode_target_protected_by_chain(32);
  for (int i = 0; i < 32; ++i) {
    decode_target_protected_by_chain[i] = i;
    some[i] = i % 2 == 0;
  }

  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/some,
                 /*is_keyframe=*/true,
                 /*frame_is_part_of_chain=*/all);
  EXPECT_NE(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/some,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/all);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/all,
                 /*is_keyframe=*/false,
                 /*frame_is_part_of_chain=*/all);
  EXPECT_NE(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

}  // namespace webrtc
