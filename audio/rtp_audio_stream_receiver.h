/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_RTP_AUDIO_STREAM_RECEIVER_H_
#define AUDIO_RTP_AUDIO_STREAM_RECEIVER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "call/audio_receive_stream.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/source/contributing_sources.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class AudioCodingModule;
class PacketRouter;
class RtcEventLog;
class RtpRtcp;
class ReceiveStatistics;

namespace voe {
class ChannelProxy;
}  // namespace voe

class RtpAudioStreamReceiver : public RtpPacketSinkInterface {
 public:
  // TODO(nisse): Same as CallStatistics from channel.h. Delete one or the
  // other.
  struct Stats {
    unsigned short fractionLost;  // NOLINT
    unsigned int cumulativeLost;
    unsigned int extendedMax;
    unsigned int jitterSamples;
    int64_t rttMs;
    size_t bytesSent;
    int packetsSent;
    size_t bytesReceived;
    int packetsReceived;
    // The capture ntp time (in local timebase) of the first played out audio
    // frame.
    int64_t capture_start_ntp_time_ms_;
  };

  RtpAudioStreamReceiver(PacketRouter* packet_router,
                         AudioReceiveStream::Config::Rtp rtp_config,
                         Transport* rtcp_send_transport,
                         AudioCodingModule* audio_coding,
                         RtcEventLog* rtc_event_log);
  ~RtpAudioStreamReceiver();

  void SetPayloadTypeFrequencies(
      std::map<uint8_t, int> payload_type_frequencies);
  void SetNACKStatus(int maxNumberOfPackets);
  void SetLocalSSRC(unsigned int ssrc);
  void StartPlayout() { is_playing_ = true; }
  void StopPlayout() { is_playing_ = false; }

  void OnRtpPacket(const RtpPacketReceived& packet) override;

  void OnRtcpPacket(rtc::ArrayView<const uint8_t> packet);

  std::vector<RtpSource> GetSources() const;
  Stats GetRtpStatistics() const;

  int64_t EstimateNtpMs(uint32_t rtp_timestamp);

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  absl::optional<Syncable::Info> GetSyncInfo() const;

  // TODO(bugs.webrtc.org/8239): When we share an RtcpTransciever,
  // it should be enough with the ssrc of the send stream.
  void AssociateSendChannel(const voe::ChannelProxy* send_channel_proxy);
  void DisassociateSendChannel();

 private:
  int64_t GetRTT(bool allow_associate_channel) const;

  PacketRouter* const packet_router_;
  // TODO(nisse): Rearrange constructor to make this pointer const.
  std::unique_ptr<RtpRtcp> rtp_rtcp_;
  // Currently too RTP specific. Should move out of RtpAudioStreamReceiver.
  AudioCodingModule* const audio_coding_;
  std::unique_ptr<ReceiveStatistics> const receive_statistics_;
  RemoteNtpTimeEstimator ntp_estimator_;  // XXX RTC_GUARDED_BY(ts_stats_lock_);
  uint32_t remote_ssrc_;

  // Indexed by payload type.
  std::map<uint8_t, int> payload_type_frequencies_;

  // Info for GetSources and GetSyncInfo is updated on network or worker thread,
  // queried on the worker thread.
  rtc::CriticalSection rtp_sources_lock_;
  ContributingSources contributing_sources_ RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint8_t> last_received_rtp_audio_level_
      RTC_GUARDED_BY(&rtp_sources_lock_);

#if 0
  rtc::CriticalSection ts_stats_lock_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);
#endif

  // TODO(nisse): Needs lock?
  bool is_playing_ = false;

  // TODO(nisse): voe::Channel had this lock, do we need it?
  // rtc::CriticalSection associated_send_channel_lock_;
  const voe::ChannelProxy* associated_send_channel_ = nullptr;
};

}  // namespace webrtc

#endif  // AUDIO_RTP_AUDIO_STREAM_RECEIVER_H_
