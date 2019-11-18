/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <utility>

#include "pc/data_channel.h"
#include "pc/peer_connection.h"
#include "pc/sctp_utils.h"

namespace webrtc {

DataChannelController::DataChannelController(PeerConnection* pc) : pc_(pc) {}

DataChannelController::~DataChannelController() {
  data_channel_transport_invoker_.reset();
}

void DataChannelController::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());
  cricket::ReceiveDataParams params;
  params.sid = channel_id;
  params.type = ToCricketDataMessageType(type);
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, buffer] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        if (!HandleOpenMessage_s(params, buffer)) {
          SignalDataChannelTransportReceivedData_s(params, buffer);
        }
      });
}

void DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosing_s(channel_id);
      });
}

void DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosed_s(channel_id);
      });
}

void DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        data_channel_transport_ready_to_send_ = true;
        SignalDataChannelTransportWritable_s(
            data_channel_transport_ready_to_send_);
      });
}

void DataChannelController::ClearSctpDataChannelsToFree() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  sctp_data_channels_to_free_.clear();
}

rtc::scoped_refptr<DataChannel>
DataChannelController::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed()) {
    return nullptr;
  }
  if (data_channel_type() == cricket::DCT_NONE) {
    RTC_LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (DataChannel::IsSctpLike(data_channel_type_)) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if ((pc_->GetSctpSslRole(&role)) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        RTC_LOG(LS_ERROR)
            << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      RTC_LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                           "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(
      DataChannel::Create(pc_, data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(channel->label()) != rtp_data_channels_.end()) {
      RTC_LOG(LS_ERROR) << "DataChannel with label " << channel->label()
                        << " already exists.";
      return nullptr;
    }
    rtp_data_channels_[channel->label()] = channel;
  } else {
    RTC_DCHECK(DataChannel::IsSctpLike(data_channel_type_));
    sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(
        this, &DataChannelController::OnSctpDataChannelClosed);
  }

  SignalDataChannelCreated_(channel.get());
  return channel;
}

bool DataChannelController::HasDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}
bool DataChannelController::HasRtpDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return !rtp_data_channels_.empty();
}
bool DataChannelController::HasSctpDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return !sctp_data_channels_.empty();
}

void DataChannelController::AllocateSctpSids(rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<DataChannel>> channels_to_close;
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        RTC_LOG(LS_ERROR) << "Failed to allocate SCTP sid, closing channel.";
        channels_to_close.push_back(channel);
        continue;
      }
      channel->SetSctpSid(sid);
    }
  }
  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptly();
  }
}

void DataChannelController::OnSctpDataChannelClosed(DataChannel* channel) {
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        // After the closing procedure is done, it's safe to use this ID for
        // another data channel.
        sid_allocator_.ReleaseSid(channel->id());
      }
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      sctp_data_channels_to_free_.push_back(*it);
      sctp_data_channels_.erase(it);
      pc_->PostMsgFreeDatachannels();
      return;
    }
  }
}

void DataChannelController::OnTransportChannelClosed() {
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  std::map<std::string, rtc::scoped_refptr<DataChannel>> temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  for (const auto& kv : temp_rtp_dcs) {
    kv.second->OnTransportChannelClosed();
  }

  std::vector<rtc::scoped_refptr<DataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelClosed();
  }
}

DataChannel* DataChannelController::FindDataChannelBySid(int sid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() == sid) {
      return channel;
    }
  }
  return nullptr;
}

bool DataChannelController::CreateDataChannel(const std::string& mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  switch (data_channel_type_) {
    case cricket::DCT_SCTP:
    case cricket::DCT_DATA_CHANNEL_TRANSPORT_SCTP:
    case cricket::DCT_DATA_CHANNEL_TRANSPORT:
    case cricket::DCT_MEDIA_TRANSPORT:
      if (!network_thread()->Invoke<bool>(
              RTC_FROM_HERE,
              rtc::Bind(&DataChannelController::SetupDataChannelTransport_n,
                        this, mid))) {
        return false;
      }

      // All non-RTP data channels must initialize |sctp_data_channels_|.
      for (const auto& channel : sctp_data_channels_) {
        channel->OnTransportChannelCreated();
      }
      return true;
    case cricket::DCT_RTP:
    default: {
      RTC_DCHECK_RUN_ON(pc_->signaling_thread());
      RtpTransportInternal* rtp_transport = pc_->GetRtpTransport(mid);
      rtp_data_channel_ = pc_->channel_manager()->CreateRtpDataChannel(
          pc_->configuration()->media_config, rtp_transport, signaling_thread(),
          mid, pc_->SrtpRequired(), pc_->GetCryptoOptions(),
          pc_->ssrc_generator());
      if (!rtp_data_channel_) {
        return false;
      }
      rtp_data_channel_->SignalDtlsSrtpSetupFailure.connect(
          pc_, &PeerConnection::OnDtlsSrtpSetupFailure);
      rtp_data_channel_->SignalSentPacket.connect(
          pc_, &PeerConnection::OnSentPacket_w);
      rtp_data_channel_->SetRtpTransport(rtp_transport);
      return true;
    }
  }
  return false;
}

bool DataChannelController::SetupDataChannelTransport_n(
    const std::string& mid) {
  DataChannelTransportInterface* transport =
      pc_->transport_controller()->GetDataChannelTransport(mid);
  if (!transport) {
    RTC_LOG(LS_ERROR)
        << "Data channel transport is not available for data channels, mid="
        << mid;
    return false;
  }
  RTC_LOG(LS_INFO) << "Setting up data channel transport for mid=" << mid;

  data_channel_transport_ = transport;
  data_channel_transport_invoker_ = std::make_unique<rtc::AsyncInvoker>();
  set_sctp_mid(mid);

  // Note: setting the data sink and checking initial state must be done last,
  // after setting up the data channel.  Setting the data sink may trigger
  // callbacks to PeerConnection which require the transport to be completely
  // set up (eg. OnReadyToSend()).
  transport->SetDataSink(this);
  return true;
}

void DataChannelController::TeardownDataChannelTransport_n() {
  if (sctp_mode() && !data_channel_transport_) {
    return;
  }
  RTC_LOG(LS_INFO) << "Tearing down data channel transport for mid="
                   << *sctp_content_name();

  // |sctp_mid_| may still be active through an SCTP transport.  If not, unset
  // it.
  clear_sctp_mid();
  data_channel_transport_invoker_ = nullptr;
  if (data_channel_transport_) {
    data_channel_transport_->SetDataSink(nullptr);
  }
  data_channel_transport_ = nullptr;
}

void DataChannelController::OnTransportChanged(
    const std::string& mid,
    DataChannelTransportInterface* data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_->SetDataSink(nullptr);
  data_channel_transport_ = data_channel_transport;
  if (data_channel_transport) {
    data_channel_transport->SetDataSink(this);

    // There's a new data channel transport.  This needs to be signaled to the
    // |sctp_data_channels_| so that they can reopen and reconnect.  This is
    // necessary when bundling is applied.
    data_channel_transport_invoker_->AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread(), [this] {
          RTC_DCHECK_RUN_ON(signaling_thread());
          for (auto channel : sctp_data_channels_) {
            channel->OnTransportChannelCreated();
          }
        });
  }
}

bool DataChannelController::ConnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (rtp_data_channel() && !data_channel_transport_) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (data_channel_transport_) {
    SignalDataChannelTransportWritable_s.connect(webrtc_data_channel,
                                                 &DataChannel::OnChannelReady);
    SignalDataChannelTransportReceivedData_s.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
    SignalDataChannelTransportChannelClosing_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureStartedRemotely);
    SignalDataChannelTransportChannelClosed_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureComplete);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.connect(
        webrtc_data_channel, &DataChannel::OnChannelReady);
    rtp_data_channel()->SignalDataReceived.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
  }
  return true;
}

rtc::Thread* DataChannelController::signaling_thread() const {
  return pc_->signaling_thread();
}
rtc::Thread* DataChannelController::network_thread() const {
  return pc_->network_thread();
}

// Add options to |session_options| from |rtp_data_channels|.
void DataChannelController::AddRtpDataChannelOptions(
    cricket::MediaDescriptionOptions* data_media_description_options) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!data_media_description_options) {
    return;
  }
  // Check for data channels.
  for (const auto& kv : rtp_data_channels_) {
    const DataChannel* channel = kv.second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // Legacy RTP data channels are signaled with the track/stream ID set to
      // the data channel's label.
      data_media_description_options->AddRtpDataChannel(channel->label(),
                                                        channel->label());
    }
  }
}

void DataChannelController::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = params.first_stream_id();
    auto data_channel_it = rtp_data_channels_.find(channel_label);
    if (data_channel_it == rtp_data_channels_.end()) {
      RTC_LOG(LS_ERROR) << "channel label not found";
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(params.first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingRtpDataChannels(existing_channels, true);
}

void DataChannelController::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = params.first_stream_id().empty()
                            ? rtc::ToString(params.first_ssrc())
                            : params.first_stream_id();
    auto data_channel_it = rtp_data_channels_.find(label);
    if (data_channel_it == rtp_data_channels_.end()) {
      // This is a new data channel.
      CreateRemoteRtpDataChannel(label, params.first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(params.first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingRtpDataChannels(existing_channels, false);
}

void DataChannelController::UpdateClosingRtpDataChannels(
    const std::vector<std::string>& active_channels,
    bool is_local_update) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  auto it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (absl::c_linear_search(active_channels, data_channel->label())) {
      ++it;
      continue;
    }

    if (is_local_update) {
      data_channel->SetSendSsrc(0);
    } else {
      data_channel->RemotePeerRequestClose();
    }

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void DataChannelController::CreateRemoteRtpDataChannel(const std::string& label,
                                                       uint32_t remote_ssrc) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, nullptr));
  if (!channel.get()) {
    RTC_LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                           "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  { pc_->Observer()->OnDataChannel(std::move(proxy_channel)); }
}

void DataChannelController::AddSctpDataStream(int sid) {
  if (data_channel_transport_) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport_) {
        data_channel_transport_->OpenChannel(sid);
      }
    });
  }
}

void DataChannelController::RemoveSctpDataStream(int sid) {
  if (data_channel_transport_) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport_) {
        data_channel_transport_->CloseChannel(sid);
      }
    });
  }
}

void DataChannelController::DisconnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (rtp_data_channel() && !data_channel_transport_) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (data_channel_transport_) {
    SignalDataChannelTransportWritable_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportReceivedData_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosing_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosed_s.disconnect(webrtc_data_channel);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.disconnect(webrtc_data_channel);
    rtp_data_channel()->SignalDataReceived.disconnect(webrtc_data_channel);
  }
}

void DataChannelController::DestroyDataChannelTransport() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (rtp_data_channel_) {
    OnTransportChannelClosed();
    pc_->DestroyChannelInterface(rtp_data_channel_);
    rtp_data_channel_ = nullptr;
  }

  // Note: Cannot use rtc::Bind to create a functor to invoke because it will
  // grab a reference to this PeerConnection. If this is called from the
  // PeerConnection destructor, the RefCountedObject vtable will have already
  // been destroyed (since it is a subclass of PeerConnection) and using
  // rtc::Bind will cause "Pure virtual function called" error to appear.

  if (sctp_mid_) {
    OnTransportChannelClosed();
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
      RTC_DCHECK_RUN_ON(network_thread());
      TeardownDataChannelTransport_n();
    });
  }
}

void DataChannelController::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, &config));
  if (!channel.get()) {
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  pc_->Observer()->OnDataChannel(std::move(proxy_channel));
  pc_->NoteUsageEvent(PeerConnection::UsageEvent::DATA_ADDED);
}

bool DataChannelController::HandleOpenMessage_s(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(buffer)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
      RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for ssrc "
                          << params.ssrc;
      return true;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    OnDataChannelOpenMessage(label, config);
    return true;
  }
  return false;
}

}  // namespace webrtc
