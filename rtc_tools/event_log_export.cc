/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <iostream>

#include <deque>
#include <map>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
enum class PacketType {
  kOutgoingPacket,
  kIncommingPacket,
  kOutgoingFeedback,
  kIncommingFeedback,
};
struct LoggedPacketPointer {
  PacketType type;
  union {
    const void* _dummy;
    const LoggedRtpPacketOutgoing* sent_packet;
    const LoggedRtpPacketIncoming* received_packet;
    const LoggedRtcpPacketTransportFeedback* sent_feedback;
    const LoggedRtcpPacketTransportFeedback* received_feedback;
  } ptr;
};

struct LogIter {
  virtual ~LogIter() = default;
  virtual int64_t GetLogTimeUs() const = 0;
  virtual LoggedPacketPointer GetLogPtr() = 0;
  virtual void Next() = 0;
  virtual bool Done() const = 0;
};

template <class T>
struct LogIterImpl : LogIter {
  LogIterImpl(PacketType type,
              const std::vector<T>& vec,
              int time_offset_ms = 0)
      : type_(type),
        iter_(vec.begin()),
        end_(vec.end()),
        time_offset_ms_(time_offset_ms) {}

  LogIterImpl() = delete;
  LogIterImpl(const LogIterImpl&) = default;

  virtual ~LogIterImpl() = default;
  int64_t GetLogTimeUs() const override {
    if (iter_ == end_)
      return INT64_MAX;
    return iter_->log_time_us() + time_offset_ms_ * 1000;
  };
  LoggedPacketPointer GetLogPtr() override {
    RTC_CHECK(iter_ != end_);
    return LoggedPacketPointer{type_,
                               {reinterpret_cast<const void*>(&(*iter_))}};
  }
  void Next() override { ++iter_; }
  bool Done() const override { return iter_ == end_; }

 private:
  const PacketType type_;
  typename std::vector<T>::const_iterator iter_;
  const typename std::vector<T>::const_iterator end_;
  int time_offset_ms_ = 0;
};

template struct LogIterImpl<LoggedRtcpPacketTransportFeedback>;
template struct LogIterImpl<LoggedRtpPacketIncoming>;
template struct LogIterImpl<LoggedRtpPacketOutgoing>;

std::vector<LoggedPacketPointer> SortedPackets(
    const ParsedRtcEventLogNew& log) {
  std::vector<std::unique_ptr<LogIter>> all_iters;

  size_t total_packet_count = 0;
  const auto& rtp_in_streams = log.incoming_rtp_packets_by_ssrc();
  for (const auto& stream : rtp_in_streams) {
    all_iters.emplace_back(new LogIterImpl<LoggedRtpPacketIncoming>(
        PacketType::kIncommingPacket, stream.incoming_packets));
    total_packet_count += stream.incoming_packets.size();
  }
  const auto& rtp_out_streams = log.outgoing_rtp_packets_by_ssrc();
  for (const auto& stream : rtp_out_streams) {
    all_iters.emplace_back(new LogIterImpl<LoggedRtpPacketOutgoing>(
        PacketType::kOutgoingPacket, stream.outgoing_packets));
    total_packet_count += stream.outgoing_packets.size();
  }

  const std::vector<LoggedRtcpPacketTransportFeedback>& in_feedbacks =
      log.transport_feedbacks(kIncomingPacket);
  all_iters.emplace_back(new LogIterImpl<LoggedRtcpPacketTransportFeedback>(
      PacketType::kIncommingFeedback, in_feedbacks));
  total_packet_count += in_feedbacks.size();

  const std::vector<LoggedRtcpPacketTransportFeedback>& out_feedbacks =
      log.transport_feedbacks(kOutgoingPacket);
  all_iters.emplace_back(new LogIterImpl<LoggedRtcpPacketTransportFeedback>(
      PacketType::kOutgoingFeedback, out_feedbacks));
  total_packet_count += out_feedbacks.size();

  std::vector<LoggedPacketPointer> sorted;
  sorted.reserve(total_packet_count);

  while (total_packet_count != sorted.size()) {
    int64_t next_packet_time_us = INT64_MAX;
    for (const auto& iter : all_iters) {
      if (!iter->Done()) {
        next_packet_time_us =
            std::min(next_packet_time_us, iter->GetLogTimeUs());
      }
    }
    for (auto& iter : all_iters) {
      while (!iter->Done() && iter->GetLogTimeUs() <= next_packet_time_us) {
        auto temp = iter->GetLogPtr();
        sorted.push_back(temp);
        iter->Next();
      }
    }
  }
  return sorted;
}

// Packets without sequence number
struct LoggedPacket {
  LoggedPacket(bool incomming, const LoggedRtpPacket& rtp)
      : incomming(incomming),
        ssrc(rtp.header.ssrc),
        stream_seq_no(rtp.header.sequenceNumber),
        size(rtp.total_length),
        capture_time(rtp.header.timestamp / 90000.0),
        log_time(rtp.log_time_us() * 1e-6) {
    if (rtp.header.extension.hasTransportSequenceNumber)
      transport_seq_no = rtp.header.extension.transportSequenceNumber;
  }
  bool incomming;
  uint32_t ssrc;
  uint16_t stream_seq_no;
  double transport_seq_no = nan("");
  size_t size;
  double capture_time;
  double log_time;
  double recv_time = nan("");
  double feedback_time = nan("");
};

void PrintHeader() {
  printf(
      "incomming ssrc stream_seq transport_seq packet_size capt_time log_time "
      "recv_time feed_time\n");
}
void FormatOut(const LoggedPacket& packet) {
  const char* incomming_str = packet.incomming ? "true" : "false";
  printf("%s %u %u %.0f %ld %.6f %.6f %.6f %.6f\n", incomming_str, packet.ssrc,
         packet.stream_seq_no, packet.transport_seq_no, packet.size,
         packet.capture_time, packet.log_time, packet.recv_time,
         packet.feedback_time);
}

class FeedbackAdapterAdapter {
 public:
  FeedbackAdapterAdapter() : clock_(10000), feedback_adapter_(&clock_) {}
  void AddRtpPacket(const LoggedRtpPacket& rtp, LoggedPacket* logged) {
    if (rtp.header.extension.hasTransportSequenceNumber) {
      uint16_t seq_num = rtp.header.extension.transportSequenceNumber;
      //      RTC_CHECK_EQ(++last_seq_num_, seq_num);
      logged->feedback_time = INFINITY;
      feedback_adapter_.AddPacket(rtp.header.ssrc, seq_num, rtp.total_length,
                                  PacedPacketInfo());
      feedback_adapter_.OnSentPacket(seq_num, rtp.header.timestamp / 1000);
      sent_tracked_[seq_num] = logged;
      sent_.push_back(logged);
    }
  }
  void UpdateWithFeedback(const LoggedRtcpPacketTransportFeedback* feedback) {
    feedback_adapter_.OnTransportFeedback(feedback->transport_feedback);
    std::vector<PacketFeedback> feedbacks =
        feedback_adapter_.GetTransportFeedbackVector();
    LoggedPacket* last_sent = nullptr;
    for (auto& fb : feedbacks) {
      if (sent_tracked_.find(fb.sequence_number) == sent_tracked_.end()) {
        RTC_LOG(LS_ERROR) << "Received feedback for unknown packet: "
                          << fb.sequence_number;
        continue;
      }
      LoggedPacket* sent = sent_tracked_[fb.sequence_number];
      auto recv_ms = fb.arrival_time_ms;
      sent->recv_time = recv_ms == -1 ? INFINITY : recv_ms * 1e-3;
      sent->feedback_time = nan("");
      last_sent = sent;
    }
    if (last_sent != nullptr) {
      last_sent->feedback_time = feedback->log_time_us() * 1e-6;
    }
  }

 private:
  //  int16_t last_seq_num_ = 0;
  SimulatedClock clock_;
  TransportFeedbackAdapter feedback_adapter_;
  std::map<int32_t, LoggedPacket*> sent_tracked_;
  std::vector<LoggedPacket*> sent_;
};

void ParseLog(const ParsedRtcEventLogNew& parsed_log_) {
  std::vector<LoggedPacketPointer> sorted_packets = SortedPackets(parsed_log_);
  std::deque<LoggedPacket> all_packets;
  FeedbackAdapterAdapter feedback_in;
  FeedbackAdapterAdapter feedback_out;

  PrintHeader();

  for (LoggedPacketPointer& packet_ptr : sorted_packets) {
    switch (packet_ptr.type) {
      case PacketType::kIncommingPacket: {
        auto& rtp = packet_ptr.ptr.received_packet->rtp;
        all_packets.emplace_back(true, rtp);
        feedback_in.AddRtpPacket(rtp, &all_packets.back());
      } break;
      case PacketType::kOutgoingFeedback: {
        auto* feedback = packet_ptr.ptr.sent_feedback;
        feedback_in.UpdateWithFeedback(feedback);
      } break;
      case PacketType::kOutgoingPacket: {
        auto& rtp = packet_ptr.ptr.sent_packet->rtp;
        all_packets.emplace_back(false, rtp);
        feedback_out.AddRtpPacket(rtp, &all_packets.back());
      } break;
      case PacketType::kIncommingFeedback: {
        auto* feedback = packet_ptr.ptr.received_feedback;
        feedback_out.UpdateWithFeedback(feedback);
      } break;
    }
  }
  for (const LoggedPacket& packet : all_packets) {
    FormatOut(packet);
  }
}
}  // namespace
}  // namespace webrtc

void PrintHelp() {
  printf("Extracts sent packets and their feedback from rtc event logs\n");
  printf("Usage: input log using filename or to stdin\n");
  printf("Output, space separated values for each sent packet.\n");
  printf("Format: ");
  printf("packet size [bytes], send time [s], ");
  printf("recv time [s/nan/inf], feedback time [s/nan]\n");
}

int main(int argc, char* argv[]) {
  webrtc::ParsedRtcEventLogNew parsed_log;
  if (argc == 2) {
    if (std::string("--help") == argv[1]) {
      PrintHelp();
    } else {
      parsed_log.ParseFile(argv[1]);
    }
  } else {
    parsed_log.ParseStream(std::cin);
  }
  webrtc::ParseLog(parsed_log);
  fflush(stdout);
  return 0;
}
