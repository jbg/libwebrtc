/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/loopback_media_transport.h"

#include <memory>

#include "absl/algorithm/container.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

WrapperMediaTransportFactory::WrapperMediaTransportFactory(
    MediaTransportFactory* wrapped)
    : wrapped_factory_(wrapped) {}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
WrapperMediaTransportFactory::CreateMediaTransport(
    rtc::PacketTransportInternal* packet_transport,
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

std::string WrapperMediaTransportFactory::GetTransportName() const {
  if (wrapped_factory_) {
    return wrapped_factory_->GetTransportName();
  }
  return "wrapped-transport";
}

int WrapperMediaTransportFactory::created_transport_count() const {
  return created_transport_count_;
}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
WrapperMediaTransportFactory::CreateMediaTransport(
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
}

#if 0
MediaTransportPair::MediaTransportPair(rtc::Thread* thread) {}
    : first_factory_(&first_datagram_transport_),
      second_factory_(&second_datagram_transport_) {}
}
#endif
MediaTransportPair::~MediaTransportPair() = default;

MediaTransportPair::LoopbackDataChannelTransport::LoopbackDataChannelTransport(
    rtc::Thread* thread)
    : thread_(thread) {}

MediaTransportPair::LoopbackDataChannelTransport::
    ~LoopbackDataChannelTransport() {
  RTC_CHECK(data_sink_ == nullptr);
}

void MediaTransportPair::LoopbackDataChannelTransport::Connect(
    LoopbackDataChannelTransport* other) {
  other_ = other;
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::OpenChannel(
    int channel_id) {
  // No-op.  No need to open channels for the loopback.
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_,
                             [this, channel_id, params, buffer] {
                               other_->OnData(channel_id, params.type, buffer);
                             });
  return RTCError::OK();
}

RTCError MediaTransportPair::LoopbackDataChannelTransport::CloseChannel(
    int channel_id) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id] {
    other_->OnRemoteCloseChannel(channel_id);
    rtc::CritScope lock(&sink_lock_);
    if (data_sink_) {
      data_sink_->OnChannelClosed(channel_id);
    }
  });
  return RTCError::OK();
}

void MediaTransportPair::LoopbackDataChannelTransport::SetDataSink(
    DataChannelSink* sink) {
  rtc::CritScope lock(&sink_lock_);
  data_sink_ = sink;
  if (data_sink_ && ready_to_send_) {
    data_sink_->OnReadyToSend();
  }
}

bool MediaTransportPair::LoopbackDataChannelTransport::IsReadyToSend() const {
  rtc::CritScope lock(&sink_lock_);
  return ready_to_send_;
}

void MediaTransportPair::LoopbackDataChannelTransport::FlushAsyncInvokes() {
  invoker_.Flush(thread_);
}

void MediaTransportPair::LoopbackDataChannelTransport::OnData(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  rtc::CritScope lock(&sink_lock_);
  if (data_sink_) {
    data_sink_->OnDataReceived(channel_id, type, buffer);
  }
}

void MediaTransportPair::LoopbackDataChannelTransport::OnRemoteCloseChannel(
    int channel_id) {
  rtc::CritScope lock(&sink_lock_);
  if (data_sink_) {
    data_sink_->OnChannelClosing(channel_id);
    data_sink_->OnChannelClosed(channel_id);
  }
}

void MediaTransportPair::LoopbackDataChannelTransport::OnReadyToSend(
    bool ready_to_send) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, ready_to_send] {
    rtc::CritScope lock(&sink_lock_);
    ready_to_send_ = ready_to_send;
    // Propagate state to data channel sink, if present.
    if (data_sink_ && ready_to_send_) {
      data_sink_->OnReadyToSend();
    }
  });
}

}  // namespace webrtc
