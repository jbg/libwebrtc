/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DATA_CHANNEL_CONTROLLER_H_
#define PC_DATA_CHANNEL_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "pc/data_channel.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread.h"

namespace cricket {
class RtpDataChannel;
struct MediaDescriptionOptions;
}  // namespace cricket

namespace webrtc {

class DataChannel;
class DataChannelTransportInterface;
struct InternalDataChannelInit;
class PeerConnection;

// The DataChannelController is the component of a PeerConnection that keeps
// track of what DataChannels exist and their state.
class DataChannelController : public DataChannelSink,
                              public sigslot::has_slots<> {
 public:
  explicit DataChannelController(PeerConnection* pc);
  ~DataChannelController();

  // Implements DataChannelSink.
  void OnDataReceived(int channel_id,
                      DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override;
  void OnChannelClosing(int channel_id) override;
  void OnChannelClosed(int channel_id) override;
  void OnReadyToSend() override;

  void OnSctpDataChannelClosed(DataChannel* channel)
      RTC_RUN_ON(signaling_thread());

  // Called when the transport for the data channels is closed or destroyed.
  void OnTransportChannelClosed() RTC_RUN_ON(signaling_thread());
  // Called when a valid data channel OPEN message is received.
  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config)
      RTC_RUN_ON(signaling_thread());

  // Parses and handles open messages.  Returns true if the message is an open
  // message, false otherwise.
  bool HandleOpenMessage_s(const cricket::ReceiveDataParams& params,
                           const rtc::CopyOnWriteBuffer& buffer)
      RTC_RUN_ON(signaling_thread());

  bool SetupDataChannelTransport_n(const std::string& mid)
      RTC_RUN_ON(network_thread());
  void TeardownDataChannelTransport_n() RTC_RUN_ON(network_thread());
  sigslot::signal1<DataChannel*> SignalDataChannelCreated_
      RTC_GUARDED_BY(signaling_thread());

  cricket::DataChannelType data_channel_type() const {
    return data_channel_type_;
  }
  void set_data_channel_type(cricket::DataChannelType data_channel_type) {
    data_channel_type_ = data_channel_type;
  }
  bool sctp_mode() const {
    // If sctp_mid_ is set, mode is SCTP.
    return sctp_mid_.has_value();
  }
  void set_sctp_mid(const std::string& mid) { sctp_mid_ = mid; }
  void clear_sctp_mid() { sctp_mid_.reset(); }

  cricket::RtpDataChannel* rtp_data_channel() const {
    return rtp_data_channel_;
  }

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return sctp_data_channels_;
  }

  absl::optional<std::string> sctp_content_name() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return sctp_mid_;
  }

  absl::optional<std::string> sctp_transport_name() const;

  sigslot::signal1<DataChannel*>& SignalDataChannelCreated() {
    return SignalDataChannelCreated_;
  }

  DataChannelTransportInterface* data_channel_transport() {
    return data_channel_transport_;
  }
  const DataChannelTransportInterface* data_channel_transport() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return data_channel_transport_;
  }
  bool data_channel_transport_ready_to_send() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return data_channel_transport_ready_to_send_;
  }

  void ClearSctpDataChannelsToFree();
  rtc::scoped_refptr<DataChannel> InternalCreateDataChannel(
      const std::string& label,
      const InternalDataChannelInit* config);
  bool HasDataChannels() const;
  bool HasRtpDataChannels() const;
  bool HasSctpDataChannels() const;
  void AllocateSctpSids(rtc::SSLRole role);
  DataChannel* FindDataChannelBySid(int sid) const;
  bool CreateDataChannel(const std::string& mid);
  void OnTransportChanged(
      const std::string& mid,
      DataChannelTransportInterface* data_channel_transport);
  void AddRtpDataChannelOptions(
      cricket::MediaDescriptionOptions* data_media_description_options) const;

  void CreateRemoteRtpDataChannel(const std::string& label,
                                  uint32_t remote_ssrc);
  void UpdateClosingRtpDataChannels(
      const std::vector<std::string>& active_channels,
      bool is_local_update);
  void UpdateRemoteRtpDataChannels(const cricket::StreamParamsVec& streams);

  void UpdateLocalRtpDataChannels(const cricket::StreamParamsVec& streams);
  void AddSctpDataStream(int sid);
  void RemoveSctpDataStream(int sid);
  void DestroyDataChannelTransport();
  bool ConnectDataChannel(DataChannel* webrtc_data_channel);
  void DisconnectDataChannel(DataChannel* webrtc_data_channel);

 private:
  rtc::Thread* signaling_thread() const;
  rtc::Thread* network_thread() const;

  // The owner PeerConnection.
  // This object is designed to be a member of PeerConnection, so pointer
  // ownership should not be a question.
  PeerConnection* pc_;

  // Specifies which kind of data channel is allowed. This is controlled
  // by the chrome command-line flag and constraints:
  // 1. If chrome command-line switch 'enable-sctp-data-channels' is enabled,
  // constraint kEnableDtlsSrtp is true, and constaint kEnableRtpDataChannels is
  // not set or false, SCTP is allowed (DCT_SCTP);
  // 2. If constraint kEnableRtpDataChannels is true, RTP is allowed (DCT_RTP);
  // 3. If both 1&2 are false, data channel is not allowed (DCT_NONE).
  cricket::DataChannelType data_channel_type_ =
      cricket::DCT_NONE;  // TODO(bugs.webrtc.org/9987): Accessed on both
                          // signaling and network thread.

  // |rtp_data_channel_| is used if in RTP data channel mode, |sctp_transport_|
  // when using SCTP. Despite the name, it is a transport, not a datachannel.
  cricket::RtpDataChannel* rtp_data_channel_ =
      nullptr;  // TODO(bugs.webrtc.org/9987): Accessed on both
                // signaling and some other thread.

  // |sctp_mid_| is the content name (MID) in SDP.
  // Note: this is used as the data channel MID by both SCTP and data channel
  // transports.  It is set when either transport is initialized and unset when
  // both transports are deleted.
  absl::optional<std::string>
      sctp_mid_;  // TODO(bugs.webrtc.org/9987): Accessed on both signaling
                  // and network thread.

  // label -> DataChannel
  std::map<std::string, rtc::scoped_refptr<DataChannel>> rtp_data_channels_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_to_free_
      RTC_GUARDED_BY(signaling_thread());

  SctpSidAllocator sid_allocator_ RTC_GUARDED_BY(signaling_thread());
  // Plugin transport used for data channels.  Pointer may be accessed and
  // checked from any thread, but the object may only be touched on the
  // network thread.
  // TODO(bugs.webrtc.org/9987): Accessed on both signaling and network thread.
  DataChannelTransportInterface* data_channel_transport_;

  // Cached value of whether the data channel transport is ready to send.
  bool data_channel_transport_ready_to_send_
      RTC_GUARDED_BY(signaling_thread()) = false;

  // Used to invoke data channel transport signals on the signaling thread.
  std::unique_ptr<rtc::AsyncInvoker> data_channel_transport_invoker_
      RTC_GUARDED_BY(network_thread());

  // Signals from |data_channel_transport_|.  These are invoked on the signaling
  // thread.
  sigslot::signal1<bool> SignalDataChannelTransportWritable_s
      RTC_GUARDED_BY(signaling_thread());
  sigslot::signal2<const cricket::ReceiveDataParams&,
                   const rtc::CopyOnWriteBuffer&>
      SignalDataChannelTransportReceivedData_s
          RTC_GUARDED_BY(signaling_thread());
  sigslot::signal1<int> SignalDataChannelTransportChannelClosing_s
      RTC_GUARDED_BY(signaling_thread());
  sigslot::signal1<int> SignalDataChannelTransportChannelClosed_s
      RTC_GUARDED_BY(signaling_thread());
};

}  // namespace webrtc
#endif  // PC_DATA_CHANNEL_CONTROLLER_H_
