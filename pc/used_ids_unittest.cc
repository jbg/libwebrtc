/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/used_ids.h"
#include "test/gtest.h"

using cricket::UsedIds;
using cricket::UsedRtpHeaderExtensionIds;

struct Foo {
  int id;
};

TEST(UsedIdsTest, UniqueIdsAreUnchanged) {
  UsedIds<Foo> used_ids(1, 5);
  for (int i = 1; i <= 5; ++i) {
    Foo id = {i};
    used_ids.FindAndSetIdUsed(&id);
    EXPECT_EQ(id.id, i);
  }
}

TEST(UsedIdsTest, CollisionsAreReassignedIdsInReverseOrder) {
  UsedIds<Foo> used_ids(1, 10);
  Foo id_1 = {1};
  Foo id_2 = {2};
  Foo id_2_collision = {2};
  Foo id_3 = {3};
  Foo id_3_collision = {3};

  used_ids.FindAndSetIdUsed(&id_1);
  used_ids.FindAndSetIdUsed(&id_2);
  used_ids.FindAndSetIdUsed(&id_2_collision);
  EXPECT_EQ(id_2_collision.id, 10);
  used_ids.FindAndSetIdUsed(&id_3);
  used_ids.FindAndSetIdUsed(&id_3_collision);
  EXPECT_EQ(id_3_collision.id, 9);
}

TEST(UsedRtpHeaderExtensionIdsDeathTest, UniqueIdsAreUnchanged) {
  bool extmap_allow_mixed[] = {false, true};
  int max_id[] = {14, 255};
  for (int i = 0; i < 2; ++i) {
    UsedRtpHeaderExtensionIds used_ids(extmap_allow_mixed[i]);

    // Fill all IDs.
    for (int j = 1; j <= max_id[i]; ++j) {
      webrtc::RtpExtension extension("", j);
      used_ids.FindAndSetIdUsed(&extension);
      EXPECT_EQ(extension.id, j);
    }
  }
}

TEST(UsedRtpHeaderExtensionIdsTest, PrioritizeReassignmentToOneByteIds) {
  bool extmap_allow_mixed[] = {false, true};
  for (int i = 0; i < 2; ++i) {
    UsedRtpHeaderExtensionIds used_ids(extmap_allow_mixed[i]);
    webrtc::RtpExtension id_1("", 1);
    webrtc::RtpExtension id_2("", 2);
    webrtc::RtpExtension id_2_collision("", 2);
    webrtc::RtpExtension id_3("", 3);
    webrtc::RtpExtension id_3_collision("", 3);

    // Expect that colliding IDs are reassigned to one-byte IDs.
    used_ids.FindAndSetIdUsed(&id_1);
    used_ids.FindAndSetIdUsed(&id_2);
    used_ids.FindAndSetIdUsed(&id_2_collision);
    EXPECT_EQ(id_2_collision.id, 14);
    used_ids.FindAndSetIdUsed(&id_3);
    used_ids.FindAndSetIdUsed(&id_3_collision);
    EXPECT_EQ(id_3_collision.id, 13);
  }
}

TEST(UsedRtpHeaderExtensionIdsTest, ExtmapAllowMixedTrueEnablesTwoByteIds) {
  UsedRtpHeaderExtensionIds used_ids(/*extmap_allow_mixed*/ true);

  // Fill all one byte IDs.
  for (int i = 1; i < 15; ++i) {
    webrtc::RtpExtension id("", i);
    used_ids.FindAndSetIdUsed(&id);
  }

  // Add new extensions with colliding IDs.
  webrtc::RtpExtension id1_collision("", 1);
  webrtc::RtpExtension id2_collision("", 2);
  webrtc::RtpExtension id3_collision("", 3);

  // Expect to reassign to two-byte header extension IDs.
  used_ids.FindAndSetIdUsed(&id1_collision);
  EXPECT_EQ(id1_collision.id, 15);
  used_ids.FindAndSetIdUsed(&id2_collision);
  EXPECT_EQ(id2_collision.id, 16);
  used_ids.FindAndSetIdUsed(&id3_collision);
  EXPECT_EQ(id3_collision.id, 17);
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(UsedIdsDeathTest, DieWhenAllIdsAreOccupied) {
  UsedIds<Foo> used_ids(1, 5);
  for (int i = 1; i <= 5; ++i) {
    Foo id = {i};
    used_ids.FindAndSetIdUsed(&id);
  }
  Foo id_collision = {3};
  EXPECT_DEATH(used_ids.FindAndSetIdUsed(&id_collision), "");
}

TEST(UsedRtpHeaderExtensionIdsDeathTest, DieWhenAllIdsAreOccupied) {
  bool extmap_allow_mixed[] = {false, true};
  int max_id[] = {14, 255};
  for (int i = 0; i < 2; ++i) {
    UsedRtpHeaderExtensionIds used_ids(extmap_allow_mixed[i]);

    // Fill all IDs.
    for (int j = 1; j <= max_id[i]; ++j) {
      webrtc::RtpExtension id("", j);
      used_ids.FindAndSetIdUsed(&id);
    }

    webrtc::RtpExtension id1_collision("", 1);
    webrtc::RtpExtension id2_collision("", 2);
    webrtc::RtpExtension id3_collision("", max_id[i]);

    EXPECT_DEATH(used_ids.FindAndSetIdUsed(&id1_collision), "");
    EXPECT_DEATH(used_ids.FindAndSetIdUsed(&id2_collision), "");
    EXPECT_DEATH(used_ids.FindAndSetIdUsed(&id3_collision), "");
  }
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
