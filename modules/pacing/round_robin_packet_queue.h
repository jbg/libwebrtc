/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
#define MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_

#include <stddef.h>
#include <stdint.h>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class RoundRobinPacketQueue {
 public:
  explicit RoundRobinPacketQueue(int64_t start_time_us);
  ~RoundRobinPacketQueue();

  class PacketInfo {
   public:
    PacketInfo();
    virtual ~PacketInfo();

    virtual RtpPacketSender::Priority Priority() const = 0;
    virtual uint32_t Ssrc() const = 0;
    virtual uint16_t SequenceNumber() const = 0;
    virtual int64_t CaptureTimeMs() const = 0;
    virtual int64_t EnqueueTimeMs() const = 0;
    virtual size_t SizeInBytes() const = 0;
    virtual bool IsRetransmission() const = 0;
    virtual uint64_t EnqueueOrder() const = 0;
    virtual std::unique_ptr<RtpPacketToSend> ReleasePacket() = 0;
  };

  void Push(RtpPacketSender::Priority priority,
            uint32_t ssrc,
            uint16_t seq_number,
            int64_t capture_time_ms,
            int64_t enqueue_time_ms,
            size_t length_in_bytes,
            bool retransmission,
            uint64_t enqueue_order);
  void Push(RtpPacketSender::Priority priority,
            int64_t enqueue_time_ms,
            bool retransmission,
            uint64_t enqueue_order,
            std::unique_ptr<RtpPacketToSend> packet);
  PacketInfo* BeginPop();
  void CancelPop();
  void FinalizePop();

  bool Empty() const;
  size_t SizeInPackets() const;
  uint64_t SizeInBytes() const;

  int64_t OldestEnqueueTimeMs() const;
  int64_t AverageQueueTimeMs() const;
  void UpdateQueueTime(int64_t timestamp_ms);
  void SetPauseState(bool paused, int64_t timestamp_ms);

 private:
  using RtpPacketIterator =
      std::list<std::unique_ptr<RtpPacketToSend>>::iterator;
  class PacketInfoImpl : public PacketInfo {
   public:
    PacketInfoImpl(RtpPacketSender::Priority priority,
                   uint32_t ssrc,
                   uint16_t seq_number,
                   int64_t capture_time_ms,
                   int64_t enqueue_time_ms,
                   size_t length_in_bytes,
                   bool retransmission,
                   uint64_t enqueue_order);
    PacketInfoImpl(const PacketInfoImpl& other);
    ~PacketInfoImpl() override;
    bool operator<(const PacketInfoImpl& other) const;

    RtpPacketSender::Priority Priority() const override;
    uint32_t Ssrc() const override;
    uint16_t SequenceNumber() const override;
    int64_t CaptureTimeMs() const override;
    int64_t EnqueueTimeMs() const override;
    size_t SizeInBytes() const override;
    bool IsRetransmission() const override;
    uint64_t EnqueueOrder() const override;
    std::unique_ptr<RtpPacketToSend> ReleasePacket() override;

    RtpPacketSender::Priority priority;
    uint32_t ssrc;
    uint16_t sequence_number;
    int64_t capture_time_ms;  // Absolute time of frame capture.
    int64_t enqueue_time_ms;  // Absolute time of pacer queue entry.
    size_t bytes;
    bool retransmission;
    uint64_t enqueue_order;
    int64_t sum_paused_ms;
    std::list<PacketInfo>::iterator this_it;
    std::multiset<int64_t>::iterator enqueue_time_it;
    // Iterator into |rtp_packets_|, where the memory for RtpPacket is owned.
    RtpPacketIterator packet_it;
  };

  struct StreamPrioKey {
    StreamPrioKey(RtpPacketSender::Priority priority, int64_t bytes)
        : priority(priority), bytes(bytes) {}

    bool operator<(const StreamPrioKey& other) const {
      if (priority != other.priority)
        return priority < other.priority;
      return bytes < other.bytes;
    }

    const RtpPacketSender::Priority priority;
    const size_t bytes;
  };

  struct Stream {
    Stream();
    Stream(const Stream&);

    virtual ~Stream();

    size_t bytes;
    uint32_t ssrc;
    std::priority_queue<PacketInfoImpl> packet_queue;

    // Whenever a packet is inserted for this stream we check if |priority_it|
    // points to an element in |stream_priorities_|, and if it does it means
    // this stream has already been scheduled, and if the scheduled priority is
    // lower than the priority of the incoming packet we reschedule this stream
    // with the higher priority.
    std::multimap<StreamPrioKey, uint32_t>::iterator priority_it;
  };

  static constexpr size_t kMaxLeadingBytes = 1400;

  void Push(PacketInfoImpl packet);

  Stream* GetHighestPriorityStream();

  // Just used to verify correctness.
  bool IsSsrcScheduled(uint32_t ssrc) const;

  int64_t time_last_updated_ms_;
  absl::optional<PacketInfoImpl> pop_packet_;
  absl::optional<Stream*> pop_stream_;

  bool paused_ = false;
  size_t size_packets_ = 0;
  size_t size_bytes_ = 0;
  size_t max_bytes_ = kMaxLeadingBytes;
  int64_t queue_time_sum_ms_ = 0;
  int64_t pause_time_sum_ms_ = 0;

  // A map of streams used to prioritize from which stream to send next. We use
  // a multimap instead of a priority_queue since the priority of a stream can
  // change as a new packet is inserted, and a multimap allows us to remove and
  // then reinsert a StreamPrioKey if the priority has increased.
  std::multimap<StreamPrioKey, uint32_t> stream_priorities_;

  // A map of SSRCs to Streams.
  std::map<uint32_t, Stream> streams_;

  // The enqueue time of every packet currently in the queue. Used to figure out
  // the age of the oldest packet in the queue.
  std::multiset<int64_t> enqueue_times_;

  // List of RTP packets to be sent, not necessarily in the order they will be
  // sent. PacketInfo.packet_it will point to an entry in this list, or the
  // end iterator of this list if queue does not have direct ownership of the
  // packet.
  std::list<std::unique_ptr<RtpPacketToSend>> rtp_packets_;
};
}  // namespace webrtc

#endif  // MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
