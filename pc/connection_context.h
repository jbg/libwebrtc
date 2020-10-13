/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CONNECTION_CONTEXT_H_
#define PC_CONNECTION_CONTEXT_H_

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "media/sctp/sctp_transport_internal.h"
#include "pc/channel_manager.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/thread.h"

namespace rtc {
class BasicNetworkManager;
class BasicPacketSocketFactory;
}  // namespace rtc

namespace webrtc {

class RtcEventLog;

// This class contains resources needed by PeerConnection and associated
// objects. A reference to this object is passed to each PeerConnection. The
// methods on this object are assumed not to change the state in any way that
// interferes with the operation of other PeerConnections.
class ConnectionContext : public rtc::RefCountInterface {
 public:
  // Functions called from PeerConnectionFactory
  void SetOptions(const PeerConnectionFactoryInterface::Options& options);

  bool Initialize();

  // Functions called from PeerConnection and friends
  SctpTransportFactoryInterface* sctp_transport_factory() {
    return sctp_factory_.get();
  }

  cricket::ChannelManager* channel_manager();

  rtc::Thread* signaling_thread() { return signaling_thread_; }
  rtc::Thread* worker_thread() { return worker_thread_; }
  rtc::Thread* network_thread() { return network_thread_; }

  const PeerConnectionFactoryInterface::Options& options() const {
    return options_;
  }

  const WebRtcKeyValueConfig& trials() const { return *trials_.get(); }

  // Accessors only used from the PeerConnectionFactory class
  rtc::BasicNetworkManager* default_network_manager() {
    return default_network_manager_.get();
  }
  rtc::BasicPacketSocketFactory* default_socket_factory() {
    return default_socket_factory_.get();
  }

 protected:
  // Thie Dependencies allows simple management of all new dependencies being
  // added to the ConnectionContext.
  explicit ConnectionContext(PeerConnectionFactoryDependencies& dependencies);

  virtual ~ConnectionContext();

 private:
  bool wraps_current_thread_;
  rtc::Thread* network_thread_;
  rtc::Thread* worker_thread_;
  rtc::Thread* signaling_thread_;
  std::unique_ptr<rtc::Thread> owned_network_thread_;
  std::unique_ptr<rtc::Thread> owned_worker_thread_;
  PeerConnectionFactoryInterface::Options options_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  const std::unique_ptr<rtc::NetworkMonitorFactory> network_monitor_factory_;
  std::unique_ptr<rtc::BasicNetworkManager> default_network_manager_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> default_socket_factory_;
  std::unique_ptr<cricket::MediaEngineInterface> media_engine_;
  std::unique_ptr<SctpTransportFactoryInterface> sctp_factory_;
  const std::unique_ptr<WebRtcKeyValueConfig> trials_;
};

}  // namespace webrtc

#endif  // PC_CONNECTION_CONTEXT_H_
