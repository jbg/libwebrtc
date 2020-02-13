/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/stats/rtcstats_objects.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "pc/media_session.h"
#include "pc/session_description.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {
namespace {
template <typename T>
void ClearTransportSeqNum(T* desc) {
  cricket::RtpHeaderExtensions extensions = desc->rtp_header_extensions();
  desc->ClearRtpHeaderExtensions();
  for (const RtpExtension& ext : extensions) {
    if (ext.uri == RtpExtension::kTransportSequenceNumberUri ||
        ext.uri == RtpExtension::kTransportSequenceNumberV2Uri)
      continue;
    desc->AddRtpHeaderExtension(ext);
  }
}
}  // namespace
TEST(ReceiveSideBweScenario, LowBitrateWithSendSideOverhead) {
  ScopedFieldTrials field_trials("WebRTC-SendSideBwe-WithOverhead/Enabled/");
  PeerScenario s(*test_info_);
  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  BuiltInNetworkBehaviorConfig net_conf;
  net_conf.link_capacity_kbps = 100;
  net_conf.queue_delay_ms = 50;
  auto node_builder = s.net()->NodeBuilder().config(net_conf);
  auto send = node_builder.Build();
  auto ret_node = node_builder.Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send.node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {send.node}, {ret_node});
  caller->CreateAudio("AUDIO", cricket::AudioOptions());
  caller->CreateVideo("VIDEO", PeerScenarioClient::VideoSendTrackConfig());
  signaling.StartIceSignaling();
  RtpHeaderExtensionMap extension_map;
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* offer) {
        ClearTransportSeqNum(
            cricket::GetFirstAudioContentDescription(offer->description()));
        ClearTransportSeqNum(
            cricket::GetFirstVideoContentDescription(offer->description()));
      },
      [&](const SessionDescriptionInterface& answer) {
        offer_exchange_done = true;
      });
  RTC_CHECK(s.WaitAndProcess(&offer_exchange_done));

  auto data_received = [&] {
    rtc::scoped_refptr<webrtc::MockRTCStatsCollectorCallback> callback(
        new rtc::RefCountedObject<webrtc::MockRTCStatsCollectorCallback>());
    callee->pc()->GetStats(callback);
    s.net()->time_controller()->Wait([&] { return callback->called(); });
    auto stats = callback->report()->GetStatsOfType<RTCTransportStats>()[0];
    return DataSize::bytes(*stats->bytes_received);
  };

  auto run_for = [&](TimeDelta runtime) {
    DataSize before = data_received();
    s.net()->time_controller()->AdvanceTime(runtime);
    DataSize after = data_received();
    return ((after - before) / runtime).kbps();
  };

  run_for(TimeDelta::seconds(5));
  EXPECT_NEAR(run_for(TimeDelta::seconds(2)), 90, 10);

  net_conf.link_capacity_kbps = 200;
  send.simulation->SetConfig(net_conf);

  run_for(TimeDelta::seconds(60));
  EXPECT_GT(run_for(TimeDelta::seconds(3)), 170);
}

}  // namespace test
}  // namespace webrtc
