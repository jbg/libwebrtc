/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/dcsctp_transport.h"

#include <memory>
#include <utility>

#include "net/dcsctp/public/mock_dcsctp_socket.h"
#include "net/dcsctp/public/mock_dcsctp_socket_factory.h"
#include "p2p/base/fake_packet_transport.h"
#include "test/gtest.h"

using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

namespace {

class SctpInternalTransportObserver : public sigslot::has_slots<> {
 public:
  MOCK_METHOD(void, OnSignalReadyToSendData, ());
  MOCK_METHOD(void, OnSignalAssociationChangeCommunicationUp, ());
  MOCK_METHOD(void, OnSignalClosingProcedureStartedRemotely, (int));
  MOCK_METHOD(void, OnSignalClosingProcedureComplete, (int));
};

struct Peer {
  Peer() : fake_packet_transport("transport"), simulated_clock(1000) {
    auto socket_ptr = std::make_unique<dcsctp::MockDcSctpSocket>();
    socket = socket_ptr.get();

    auto mock_dcsctp_socket_factory =
        std::make_unique<dcsctp::MockDcSctpSocketFactory>();
    EXPECT_CALL(*mock_dcsctp_socket_factory, Create)
        .Times(1)
        .WillOnce(Return(ByMove(std::move(socket_ptr))));

    sctp_transport = std::make_unique<webrtc::DcSctpTransport>(
        rtc::Thread::Current(), &fake_packet_transport, &simulated_clock,
        std::move(mock_dcsctp_socket_factory));

    sctp_transport->SignalAssociationChangeCommunicationUp.connect(
        static_cast<SctpInternalTransportObserver*>(&observer),
        &SctpInternalTransportObserver::OnSignalReadyToSendData);
    sctp_transport->SignalAssociationChangeCommunicationUp.connect(
        static_cast<SctpInternalTransportObserver*>(&observer),
        &SctpInternalTransportObserver::
            OnSignalAssociationChangeCommunicationUp);
    sctp_transport->SignalClosingProcedureStartedRemotely.connect(
        static_cast<SctpInternalTransportObserver*>(&observer),
        &SctpInternalTransportObserver::
            OnSignalClosingProcedureStartedRemotely);
    sctp_transport->SignalClosingProcedureComplete.connect(
        static_cast<SctpInternalTransportObserver*>(&observer),
        &SctpInternalTransportObserver::OnSignalClosingProcedureComplete);
  }

  rtc::FakePacketTransport fake_packet_transport;
  webrtc::SimulatedClock simulated_clock;
  dcsctp::MockDcSctpSocket* socket;
  std::unique_ptr<webrtc::DcSctpTransport> sctp_transport;
  NiceMock<SctpInternalTransportObserver> observer;
};
}  // namespace

class DcSctpTransportTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(DcSctpTransportTest, OpenSequence) {
  Peer peer_a;
  peer_a.fake_packet_transport.SetWritable(true);

  EXPECT_CALL(*peer_a.socket, Connect)
      .Times(1)
      .WillOnce(Invoke(peer_a.sctp_transport.get(),
                       &dcsctp::DcSctpSocketCallbacks::OnConnected));
  EXPECT_CALL(peer_a.observer, OnSignalReadyToSendData);
  EXPECT_CALL(peer_a.observer, OnSignalAssociationChangeCommunicationUp);

  peer_a.sctp_transport->Start(5000, 5000, 256 * 1024);
}

TEST_F(DcSctpTransportTest, CloseSequence) {
  Peer peer_a;
  Peer peer_b;
  peer_a.fake_packet_transport.SetDestination(&peer_b.fake_packet_transport,
                                              false);
  {
    InSequence sequence;

    EXPECT_CALL(*peer_a.socket, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(DoAll(
            Invoke(peer_b.sctp_transport.get(),
                   &dcsctp::DcSctpSocketCallbacks::OnIncomingStreamsReset),
            Invoke(peer_a.sctp_transport.get(),
                   &dcsctp::DcSctpSocketCallbacks::OnStreamsResetPerformed),
            Return(dcsctp::ResetStreamsStatus::kPerformed)));

    EXPECT_CALL(*peer_b.socket, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(DoAll(
            Invoke(peer_a.sctp_transport.get(),
                   &dcsctp::DcSctpSocketCallbacks::OnIncomingStreamsReset),
            Invoke(peer_b.sctp_transport.get(),
                   &dcsctp::DcSctpSocketCallbacks::OnStreamsResetPerformed),
            Return(dcsctp::ResetStreamsStatus::kPerformed)));

    EXPECT_CALL(peer_a.observer, OnSignalClosingProcedureComplete(1));
    EXPECT_CALL(peer_b.observer, OnSignalClosingProcedureComplete(1));
    EXPECT_CALL(peer_b.observer, OnSignalClosingProcedureStartedRemotely(1));
  }

  peer_a.sctp_transport->Start(5000, 5000, 256 * 1024);
  peer_b.sctp_transport->Start(5000, 5000, 256 * 1024);
  peer_a.sctp_transport->OpenStream(1);
  peer_a.sctp_transport->ResetStream(1);
}

}  // namespace webrtc
