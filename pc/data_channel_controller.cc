/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the implementation of the class
// webrtc::PeerConnection::DataChannelController.
//
// The intent is that this should be webrtc::DataChannelController, but
// as a migration stage, it is simpler to have it as an inner class,
// declared in the header file pc/peer_connection.h

#include "pc/peer_connection.h"
#include "pc/sctp_utils.h"

namespace webrtc {

bool PeerConnection::DataChannelController::SendData(
    const cricket::SendDataParams& params,
    const rtc::CopyOnWriteBuffer& payload,
    cricket::SendDataResult* result) {
  // RTC_DCHECK_RUN_ON(signaling_thread());
  if (data_channel_transport()) {
    SendDataParams send_params;
    send_params.type = ToWebrtcDataMessageType(params.type);
    send_params.ordered = params.ordered;
    if (params.max_rtx_count >= 0) {
      send_params.max_rtx_count = params.max_rtx_count;
    } else if (params.max_rtx_ms >= 0) {
      send_params.max_rtx_ms = params.max_rtx_ms;
    }

    RTCError error = network_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, [this, params, send_params, payload] {
          return data_channel_transport()->SendData(params.sid, send_params,
                                                    payload);
        });

    if (error.ok()) {
      *result = cricket::SendDataResult::SDR_SUCCESS;
      return true;
    } else if (error.type() == RTCErrorType::RESOURCE_EXHAUSTED) {
      // SCTP transport uses RESOURCE_EXHAUSTED when it's blocked.
      // TODO(mellem):  Stop using RTCError here and get rid of the mapping.
      *result = cricket::SendDataResult::SDR_BLOCK;
      return false;
    }
    *result = cricket::SendDataResult::SDR_ERROR;
    return false;
  } else if (rtp_data_channel()) {
    return rtp_data_channel()->SendData(params, payload, result);
  }
  RTC_LOG(LS_ERROR) << "SendData called before transport is ready";
  return false;
}

bool PeerConnection::DataChannelController::ConnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (data_channel_transport()) {
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

void PeerConnection::DataChannelController::DisconnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (data_channel_transport()) {
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

void PeerConnection::DataChannelController::AddSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->OpenChannel(sid);
      }
    });
  }
}

void PeerConnection::DataChannelController::RemoveSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->CloseChannel(sid);
      }
    });
  }
}

bool PeerConnection::DataChannelController::ReadyToSendData() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return (rtp_data_channel() && rtp_data_channel()->ready_to_send_data()) ||
         (data_channel_transport() && data_channel_transport_ready_to_send_);
}

void PeerConnection::DataChannelController::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());
  cricket::ReceiveDataParams params;
  params.sid = channel_id;
  params.type = ToCricketDataMessageType(type);
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, buffer] {
        RTC_DCHECK_RUN_ON(pc_->signaling_thread());
        if (!pc_->HandleOpenMessage_s(params, buffer)) {
          RTC_DCHECK_RUN_ON(signaling_thread());
          SignalDataChannelTransportReceivedData_s(params, buffer);
        }
      });
}

void PeerConnection::DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosing_s(channel_id);
      });
}

void PeerConnection::DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosed_s(channel_id);
      });
}

void PeerConnection::DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        data_channel_transport_ready_to_send_ = true;
        SignalDataChannelTransportWritable_s(
            data_channel_transport_ready_to_send_);
      });
}

void PeerConnection::DataChannelController::OnTransportChanged(
    const std::string& mid,
    RtpTransportInternal* rtp_transport,
    rtc::scoped_refptr<DtlsTransport> dtls_transport,
    DataChannelTransportInterface* new_data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport() && mid == pc_->sctp_mid_ &&
      data_channel_transport() != new_data_channel_transport) {
    // Changed which data channel transport is used for |sctp_mid_| (eg. now
    // it's bundled).
    data_channel_transport()->SetDataSink(nullptr);
    set_data_channel_transport(new_data_channel_transport);
    if (new_data_channel_transport) {
      new_data_channel_transport->SetDataSink(&pc_->data_channel_controller_);

      // There's a new data channel transport.  This needs to be signaled to the
      // |sctp_data_channels_| so that they can reopen and reconnect.  This is
      // necessary when bundling is applied.
      data_channel_transport_invoker_->AsyncInvoke<void>(
          RTC_FROM_HERE, signaling_thread(), [this] {
            RTC_DCHECK_RUN_ON(pc_->signaling_thread());
            for (auto channel : pc_->sctp_data_channels_) {
              channel->OnTransportChannelCreated();
            }
          });
    }
  }
}

}  // namespace webrtc
