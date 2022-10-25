/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/analyzing_video_sinks_support.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::Eq;

using VideoConfig =
    ::webrtc::webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::VideoConfig;

TEST(AnalyzingVideoSinksSupportTest, ConfigsCanBeAdded) {
  VideoConfig config("alice_video", /*width=*/1280, /*height=*/720, /*fps=*/30);

  AnalyzingVideoSinksSupport support;
  support.AddConfig("alice", config);

  absl::optional<std::pair<std::string, VideoConfig>> registred_config =
      support.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  EXPECT_THAT(registred_config->second.stream_label, Eq(config.stream_label));
  EXPECT_THAT(registred_config->second.width, Eq(config.width));
  EXPECT_THAT(registred_config->second.height, Eq(config.height));
  EXPECT_THAT(registred_config->second.fps, Eq(config.fps));
}

TEST(AnalyzingVideoSinksSupportTest, AddingForExistingLabelWillOverwriteValue) {
  VideoConfig config_before("alice_video", /*width=*/1280, /*height=*/720,
                            /*fps=*/30);
  VideoConfig config_after("alice_video", /*width=*/640, /*height=*/360,
                           /*fps=*/15);

  AnalyzingVideoSinksSupport support;
  support.AddConfig("alice", config_before);

  absl::optional<std::pair<std::string, VideoConfig>> registred_config =
      support.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  EXPECT_THAT(registred_config->second.stream_label,
              Eq(config_before.stream_label));
  EXPECT_THAT(registred_config->second.width, Eq(config_before.width));
  EXPECT_THAT(registred_config->second.height, Eq(config_before.height));
  EXPECT_THAT(registred_config->second.fps, Eq(config_before.fps));

  support.AddConfig("alice", config_after);

  registred_config = support.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  EXPECT_THAT(registred_config->second.stream_label,
              Eq(config_after.stream_label));
  EXPECT_THAT(registred_config->second.width, Eq(config_after.width));
  EXPECT_THAT(registred_config->second.height, Eq(config_after.height));
  EXPECT_THAT(registred_config->second.fps, Eq(config_after.fps));
}

TEST(AnalyzingVideoSinksSupportTest, ConfigsCanBeRemoved) {
  VideoConfig config("alice_video", /*width=*/1280, /*height=*/720, /*fps=*/30);

  AnalyzingVideoSinksSupport support;
  support.AddConfig("alice", config);

  ASSERT_TRUE(support.GetPeerAndConfig("alice_video").has_value());

  support.RemoveConfig("alice_video");
  ASSERT_FALSE(support.GetPeerAndConfig("alice_video").has_value());
}

TEST(AnalyzingVideoSinksSupportTest, RemoveOfNonExistingConfigDontCrash) {
  AnalyzingVideoSinksSupport support;
  support.RemoveConfig("alice_video");
}

TEST(AnalyzingVideoSinksSupportTest, ClearRemovesAllConfigs) {
  VideoConfig config1("alice_video", /*width=*/640, /*height=*/360, /*fps=*/30);
  VideoConfig config2("bob_video", /*width=*/640, /*height=*/360, /*fps=*/30);

  AnalyzingVideoSinksSupport support;
  support.AddConfig("alice", config1);
  support.AddConfig("bob", config2);

  ASSERT_TRUE(support.GetPeerAndConfig("alice_video").has_value());
  ASSERT_TRUE(support.GetPeerAndConfig("bob_video").has_value());

  support.Clear();
  ASSERT_FALSE(support.GetPeerAndConfig("alice_video").has_value());
  ASSERT_FALSE(support.GetPeerAndConfig("bob_video").has_value());
}

struct TestVideoFrameWriterFactory {
  int closed_writers_count = 0;
  int deleted_writers_count = 0;

  std::unique_ptr<test::VideoFrameWriter> CreateWriter() {
    return std::make_unique<TestVideoFrameWriter>(this);
  }

 private:
  class TestVideoFrameWriter : public test::VideoFrameWriter {
   public:
    explicit TestVideoFrameWriter(TestVideoFrameWriterFactory* factory)
        : factory_(factory) {}
    ~TestVideoFrameWriter() override { factory_->deleted_writers_count++; }

    bool WriteFrame(const VideoFrame& frame) override { return true; }

    void Close() override { factory_->closed_writers_count++; }

   private:
    TestVideoFrameWriterFactory* factory_;
  };
};

TEST(AnalyzingVideoSinksSupportTest, RemovingWritersCloseAndDestroyAllOfThem) {
  TestVideoFrameWriterFactory factory;

  AnalyzingVideoSinksSupport support;
  test::VideoFrameWriter* writer1 =
      support.AddVideoWriter(factory.CreateWriter());
  test::VideoFrameWriter* writer2 =
      support.AddVideoWriter(factory.CreateWriter());

  support.CloseAndRemoveVideoWriters({writer1, writer2});

  EXPECT_THAT(factory.closed_writers_count, Eq(2));
  EXPECT_THAT(factory.deleted_writers_count, Eq(2));
}

TEST(AnalyzingVideoSinksSupportTest, ClearCloseAndDestroyAllWriters) {
  TestVideoFrameWriterFactory factory;

  AnalyzingVideoSinksSupport support;
  support.AddVideoWriter(factory.CreateWriter());
  support.AddVideoWriter(factory.CreateWriter());

  support.Clear();

  EXPECT_THAT(factory.closed_writers_count, Eq(2));
  EXPECT_THAT(factory.deleted_writers_count, Eq(2));
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
