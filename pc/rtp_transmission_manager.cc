/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_transmission_manager.h"

#include <algorithm>

#include "absl/types/optional.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "pc/audio_rtp_receiver.h"
#include "pc/stats_collector_interface.h"
#include "pc/video_rtp_receiver.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

RtpTransmissionManager::RtpTransmissionManager(
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    cricket::ChannelManager* channel_manager,
    UsagePattern* usage_pattern,
    PeerConnectionObserver* observer,
    StatsCollectorInterface* stats,
    std::function<void()> on_negotiation_needed)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      channel_manager_(channel_manager),
      usage_pattern_(usage_pattern),
      observer_(observer),
      stats_(stats),
      on_negotiation_needed_(on_negotiation_needed) {}

void RtpTransmissionManager::Close() {
  closed_ = true;
  observer_ = nullptr;
}

// Implementation of SetStreamsObserver
void RtpTransmissionManager::OnSetStreams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
    OnNegotiationNeeded();
}

// Function to call back to the PeerConnection when negotiation is needed
void RtpTransmissionManager::OnNegotiationNeeded() {
  on_negotiation_needed_();
}

// Function that returns the currently valid observer
PeerConnectionObserver* RtpTransmissionManager::Observer() const {
  RTC_DCHECK(!closed_);
  RTC_DCHECK(observer_);
  return observer_;
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
RtpTransmissionManager::AddTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  return AddTrackUnifiedPlan(track, stream_ids);
}

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
RtpTransmissionManager::AddTrackUnifiedPlan(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  auto transceiver = FindFirstTransceiverForAddedTrack(track);
  if (transceiver) {
    RTC_LOG(LS_INFO) << "Reusing an existing "
                     << cricket::MediaTypeToString(transceiver->media_type())
                     << " transceiver for AddTrack.";
    if (transceiver->stopping()) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "The existing transceiver is stopping.");
    }

    if (transceiver->direction() == RtpTransceiverDirection::kRecvOnly) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendRecv);
    } else if (transceiver->direction() == RtpTransceiverDirection::kInactive) {
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kSendOnly);
    }
    transceiver->sender()->SetTrack(track);
    transceiver->internal()->sender_internal()->set_stream_ids(stream_ids);
    transceiver->internal()->set_reused_for_addtrack(true);
  } else {
    cricket::MediaType media_type =
        (track->kind() == MediaStreamTrackInterface::kAudioKind
             ? cricket::MEDIA_TYPE_AUDIO
             : cricket::MEDIA_TYPE_VIDEO);
    RTC_LOG(LS_INFO) << "Adding " << cricket::MediaTypeToString(media_type)
                     << " transceiver in response to a call to AddTrack.";
    std::string sender_id = track->id();
    // Avoid creating a sender with an existing ID by generating a random ID.
    // This can happen if this is the second time AddTrack has created a sender
    // for this track.
    if (FindSenderById(sender_id)) {
      sender_id = rtc::CreateRandomUuid();
    }
    auto sender = CreateSender(media_type, sender_id, track, stream_ids, {});
    auto receiver = CreateReceiver(media_type, rtc::CreateRandomUuid());
    transceiver = CreateAndAddTransceiver(sender, receiver);
    transceiver->internal()->set_created_by_addtrack(true);
    transceiver->internal()->set_direction(RtpTransceiverDirection::kSendRecv);
  }
  return transceiver->sender();
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
RtpTransmissionManager::CreateSender(
    cricket::MediaType media_type,
    const std::string& id,
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& send_encodings) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kAudioKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        AudioRtpSender::Create(worker_thread(), id, stats_, this));
    NoteUsageEvent(UsageEvent::AUDIO_ADDED);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    RTC_DCHECK(!track ||
               (track->kind() == MediaStreamTrackInterface::kVideoKind));
    sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), VideoRtpSender::Create(worker_thread(), id, this));
    NoteUsageEvent(UsageEvent::VIDEO_ADDED);
  }
  bool set_track_succeeded = sender->SetTrack(track);
  RTC_DCHECK(set_track_succeeded);
  sender->internal()->set_stream_ids(stream_ids);
  sender->internal()->set_init_send_encodings(send_encodings);
  return sender;
}

rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
RtpTransmissionManager::CreateReceiver(cricket::MediaType media_type,
                                       const std::string& receiver_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(), new AudioRtpReceiver(worker_thread(), receiver_id,
                                                 std::vector<std::string>({})));
    NoteUsageEvent(UsageEvent::AUDIO_ADDED);
  } else {
    RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
    receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
        signaling_thread(), new VideoRtpReceiver(worker_thread(), receiver_id,
                                                 std::vector<std::string>({})));
    NoteUsageEvent(UsageEvent::VIDEO_ADDED);
  }
  return receiver;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
RtpTransmissionManager::CreateAndAddTransceiver(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Ensure that the new sender does not have an ID that is already in use by
  // another sender.
  // Allow receiver IDs to conflict since those come from remote SDP (which
  // could be invalid, but should not cause a crash).
  RTC_DCHECK(!FindSenderById(sender->id()));
  auto transceiver = RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
      signaling_thread(),
      new RtpTransceiver(
          sender, receiver, channel_manager(),
          sender->media_type() == cricket::MEDIA_TYPE_AUDIO
              ? channel_manager()->GetSupportedAudioRtpHeaderExtensions()
              : channel_manager()->GetSupportedVideoRtpHeaderExtensions()));
  transceivers()->Add(transceiver);
  transceiver->internal()->SignalNegotiationNeeded.connect(
      this, &RtpTransmissionManager::OnNegotiationNeeded);
  return transceiver;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
RtpTransmissionManager::FindFirstTransceiverForAddedTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(track);
  for (auto transceiver : transceivers()->List()) {
    if (!transceiver->sender()->track() &&
        cricket::MediaTypeToString(transceiver->media_type()) ==
            track->kind() &&
        !transceiver->internal()->has_ever_been_used_to_send() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
RtpTransmissionManager::GetSendersInternal() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      all_senders;
  for (const auto& transceiver : transceivers_.List()) {
    if (transceiver->internal()->stopped())
      continue;

    auto senders = transceiver->internal()->senders();
    all_senders.insert(all_senders.end(), senders.begin(), senders.end());
  }
  return all_senders;
}

std::vector<
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
RtpTransmissionManager::GetReceiversInternal() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      all_receivers;
  for (const auto& transceiver : transceivers_.List()) {
    if (transceiver->internal()->stopped())
      continue;

    auto receivers = transceiver->internal()->receivers();
    all_receivers.insert(all_receivers.end(), receivers.begin(),
                         receivers.end());
  }
  return all_receivers;
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
RtpTransmissionManager::GetFirstAudioTransceiver() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (auto transceiver : transceivers_.List()) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      return transceiver;
    }
  }
  return nullptr;
}

void RtpTransmissionManager::OnLocalSenderRemoved(
    const RtpSenderInfo& sender_info,
    cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  auto sender = FindSenderById(sender_info.sender_id);
  if (!sender) {
    // This is the normal case. I.e., RemoveStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }

  // A sender has been removed from the SessionDescription but it's still
  // associated with the PeerConnection. This only occurs if the SDP doesn't
  // match with the calls to CreateSender, AddStream and RemoveStream.
  if (sender->media_type() != media_type) {
    RTC_LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                           " description with an unexpected media type.";
    return;
  }

  sender->internal()->SetSsrc(0);
}

std::vector<RtpSenderInfo>* RtpTransmissionManager::GetRemoteSenderInfos(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO)
             ? &remote_audio_sender_infos_
             : &remote_video_sender_infos_;
}

std::vector<RtpSenderInfo>* RtpTransmissionManager::GetLocalSenderInfos(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &local_audio_sender_infos_
                                                   : &local_video_sender_infos_;
}

const RtpSenderInfo* RtpTransmissionManager::FindSenderInfo(
    const std::vector<RtpSenderInfo>& infos,
    const std::string& stream_id,
    const std::string sender_id) const {
  for (const RtpSenderInfo& sender_info : infos) {
    if (sender_info.stream_id == stream_id &&
        sender_info.sender_id == sender_id) {
      return &sender_info;
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
RtpTransmissionManager::FindSenderForTrack(
    MediaStreamTrackInterface* track) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& transceiver : transceivers_.List()) {
    for (auto sender : transceiver->internal()->senders()) {
      if (sender->track() == track) {
        return sender;
      }
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
RtpTransmissionManager::FindSenderById(const std::string& sender_id) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& transceiver : transceivers_.List()) {
    for (auto sender : transceiver->internal()->senders()) {
      if (sender->id() == sender_id) {
        return sender;
      }
    }
  }
  return nullptr;
}

rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
RtpTransmissionManager::FindReceiverById(const std::string& receiver_id) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& transceiver : transceivers_.List()) {
    for (auto receiver : transceiver->internal()->receivers()) {
      if (receiver->id() == receiver_id) {
        return receiver;
      }
    }
  }
  return nullptr;
}

}  // namespace webrtc
