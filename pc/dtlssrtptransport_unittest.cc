/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtlssrtptransport.h"

#include <memory>
#include <utility>

#include "media/base/fakertp.h"
#include "p2p/base/dtlstransportinternal.h"
#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakepackettransport.h"
#include "pc/rtptransport.h"
#include "pc/rtptransporttestutil.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sslstreamadapter.h"

using cricket::FakeDtlsTransport;
using cricket::FakeIceTransport;
using webrtc::DtlsSrtpTransport;
using webrtc::SrtpTransport;
using webrtc::RtpTransport;

const int kRtpAuthTagLen = 10;

class DtlsSrtpTransportTest : public testing::Test,
                              public sigslot::has_slots<> {
 protected:
  DtlsSrtpTransportTest() {
    bool rtcp_mux_enabled = true;
    // Setup RtpTransport.
    auto rtp_transport1 = rtc::MakeUnique<RtpTransport>(rtcp_mux_enabled);
    auto rtp_transport2 = rtc::MakeUnique<RtpTransport>(rtcp_mux_enabled);
    fake_ice_transport1_ =
        rtc::MakeUnique<FakeIceTransport>("fake_ice_transport1", 1);
    fake_ice_transport2_ =
        rtc::MakeUnique<FakeIceTransport>("fake_ice_transport2", 1);

    fake_dtls_transport1_ =
        rtc::MakeUnique<FakeDtlsTransport>(fake_ice_transport1_.get());
    fake_dtls_transport2_ =
        rtc::MakeUnique<FakeDtlsTransport>(fake_ice_transport2_.get());
    rtp_transport1->SetRtpPacketTransport(fake_dtls_transport1_.get());
    rtp_transport2->SetRtpPacketTransport(fake_dtls_transport2_.get());
    // Add payload type for RTP packet and RTCP packet.
    rtp_transport1->AddHandledPayloadType(0x00);
    rtp_transport2->AddHandledPayloadType(0x00);
    rtp_transport1->AddHandledPayloadType(0xc9);
    rtp_transport2->AddHandledPayloadType(0xc9);

    // Setup the SrtpTransport wrapping an RtpTransport.
    auto srtp_transport1 =
        rtc::MakeUnique<SrtpTransport>(std::move(rtp_transport1), "content");
    auto srtp_transport2 =
        rtc::MakeUnique<SrtpTransport>(std::move(rtp_transport2), "content");

    // Setup the DtlsSrtpTransport wrapping an SrtpTransport.
    dtls_srtp_transport1_ =
        rtc::MakeUnique<DtlsSrtpTransport>(std::move(srtp_transport1));
    dtls_srtp_transport2_ =
        rtc::MakeUnique<DtlsSrtpTransport>(std::move(srtp_transport2));
    dtls_srtp_transport1_->SetRtpDtlsTransport(fake_dtls_transport1_.get());
    dtls_srtp_transport2_->SetRtpDtlsTransport(fake_dtls_transport2_.get());

    dtls_srtp_transport1_->SignalPacketReceived.connect(
        this, &DtlsSrtpTransportTest::OnPacketReceived1);
    dtls_srtp_transport2_->SignalPacketReceived.connect(
        this, &DtlsSrtpTransportTest::OnPacketReceived2);
  }

  void SetupDtlsSrtp() {
    // Setting certifications for DtlsTransport.
    auto cert1 = rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
        rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
    fake_dtls_transport1_->SetLocalCertificate(cert1);
    auto cert2 = rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
        rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
    fake_dtls_transport2_->SetLocalCertificate(cert2);
    fake_dtls_transport1_->SetDestination(fake_dtls_transport2_.get());
    // The DtlsSrtpTransport would try to setup the DTLS-SRTP when
    // |SetRtcpMuxEnabled| is called.
    dtls_srtp_transport1_->SetRtcpMuxEnabled(true);
    dtls_srtp_transport2_->SetRtcpMuxEnabled(true);
    EXPECT_TRUE(dtls_srtp_transport1_->IsActive());
    EXPECT_TRUE(dtls_srtp_transport2_->IsActive());
  }

  void OnPacketReceived1(bool rtcp,
                         rtc::CopyOnWriteBuffer* packet,
                         const rtc::PacketTime& packet_time) {
    LOG(LS_INFO) << "DtlsSrtpTransport1 received a packet.";
    last_recv_packet1_ = *packet;
  }

  void OnPacketReceived2(bool rtcp,
                         rtc::CopyOnWriteBuffer* packet,
                         const rtc::PacketTime& packet_time) {
    LOG(LS_INFO) << "DtlsSrtpTransport2 received a packet.";
    last_recv_packet2_ = *packet;
  }

  std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport1_;
  std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport2_;
  std::unique_ptr<FakeDtlsTransport> fake_dtls_transport1_;
  std::unique_ptr<FakeDtlsTransport> fake_dtls_transport2_;
  std::unique_ptr<FakeIceTransport> fake_ice_transport1_;
  std::unique_ptr<FakeIceTransport> fake_ice_transport2_;

  rtc::CopyOnWriteBuffer last_recv_packet1_;
  rtc::CopyOnWriteBuffer last_recv_packet2_;
  int sequence_number_ = 0;
};

TEST_F(DtlsSrtpTransportTest, SendRecvPacket) {
  SetupDtlsSrtp();
  size_t rtp_len = sizeof(kPcmuFrame);
  size_t packet_size = rtp_len + kRtpAuthTagLen;
  rtc::Buffer rtp_packet_buffer(packet_size);
  char* rtp_packet_data = rtp_packet_buffer.data<char>();
  memcpy(rtp_packet_data, kPcmuFrame, rtp_len);
  // In order to be able to run this test function multiple times we can not
  // use the same sequence number twice. Increase the sequence number by one.
  rtc::SetBE16(reinterpret_cast<uint8_t*>(rtp_packet_data) + 2,
               ++sequence_number_);
  rtc::CopyOnWriteBuffer rtp_packet1to2(rtp_packet_data, rtp_len, packet_size);
  rtc::CopyOnWriteBuffer rtp_packet2to1(rtp_packet_data, rtp_len, packet_size);

  rtc::PacketOptions options;
  // Send a packet from |srtp_transport1_| to |srtp_transport2_| and verify
  // that the packet can be successfully received and decrypted.
  ASSERT_TRUE(dtls_srtp_transport1_->SendRtpPacket(&rtp_packet1to2, options,
                                                   cricket::PF_SRTP_BYPASS));
  EXPECT_EQ(0, memcmp(last_recv_packet2_.data(), kPcmuFrame, rtp_len));
  ASSERT_TRUE(dtls_srtp_transport2_->SendRtpPacket(&rtp_packet2to1, options,
                                                   cricket::PF_SRTP_BYPASS));
  EXPECT_EQ(0, memcmp(last_recv_packet1_.data(), kPcmuFrame, rtp_len));
}
