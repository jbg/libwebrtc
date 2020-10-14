/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/connection_context.h"

#include <utility>

#include "api/transport/field_trial_based_config.h"
#include "media/base/rtp_data_engine.h"

namespace webrtc {

ConnectionContext::ConnectionContext(
    PeerConnectionFactoryDependencies& dependencies)
    : wraps_current_thread_(false),
      network_thread_(dependencies.network_thread),
      worker_thread_(dependencies.worker_thread),
      signaling_thread_(dependencies.signaling_thread),
      network_monitor_factory_(std::move(dependencies.network_monitor_factory)),
      call_factory_(std::move(dependencies.call_factory)),
      media_engine_(std::move(dependencies.media_engine)),
      sctp_factory_(std::move(dependencies.sctp_factory)),
      trials_(dependencies.trials ? std::move(dependencies.trials)
                                  : std::make_unique<FieldTrialBasedConfig>()) {
  if (!network_thread_) {
    owned_network_thread_ = rtc::Thread::CreateWithSocketServer();
    owned_network_thread_->SetName("pc_network_thread", nullptr);
    owned_network_thread_->Start();
    network_thread_ = owned_network_thread_.get();
  }

  if (!worker_thread_) {
    owned_worker_thread_ = rtc::Thread::Create();
    owned_worker_thread_->SetName("pc_worker_thread", nullptr);
    owned_worker_thread_->Start();
    worker_thread_ = owned_worker_thread_.get();
  }

  if (!signaling_thread_) {
    signaling_thread_ = rtc::Thread::Current();
    if (!signaling_thread_) {
      // If this thread isn't already wrapped by an rtc::Thread, create a
      // wrapper and own it in this class.
      signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
      wraps_current_thread_ = true;
    }
  }
  signaling_thread_->AllowInvokesToThread(worker_thread_);
  signaling_thread_->AllowInvokesToThread(network_thread_);
  worker_thread_->AllowInvokesToThread(network_thread_);
  network_thread_->DisallowAllInvokes();

#ifdef HAVE_SCTP
  if (!sctp_factory_) {
    sctp_factory_ =
        std::make_unique<cricket::SctpTransportFactory>(network_thread());
  }
#endif
}

ConnectionContext::~ConnectionContext() {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  channel_manager_.reset(nullptr);

  // Make sure |worker_thread()| and |signaling_thread()| outlive
  // |default_socket_factory_| and |default_network_manager_|.
  default_socket_factory_ = nullptr;
  default_network_manager_ = nullptr;

  if (wraps_current_thread_)
    rtc::ThreadManager::Instance()->UnwrapCurrentThread();
}

void ConnectionContext::SetOptions(
    const PeerConnectionFactoryInterface::Options& options) {
  options_ = options;
}

bool ConnectionContext::Initialize() {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  rtc::InitRandom(rtc::Time32());

  // If network_monitor_factory_ is non-null, it will be used to create a
  // network monitor while on the network thread.
  default_network_manager_.reset(
      new rtc::BasicNetworkManager(network_monitor_factory_.get()));
  if (!default_network_manager_) {
    return false;
  }

  default_socket_factory_.reset(
      new rtc::BasicPacketSocketFactory(network_thread()));
  if (!default_socket_factory_) {
    return false;
  }

  channel_manager_ = std::make_unique<cricket::ChannelManager>(
      std::move(media_engine_), std::make_unique<cricket::RtpDataEngine>(),
      worker_thread(), network_thread());

  channel_manager_->SetVideoRtxEnabled(true);
  if (!channel_manager_->Init()) {
    return false;
  }

  return true;
}

cricket::ChannelManager* ConnectionContext::channel_manager() const {
  return channel_manager_.get();
}

}  // namespace webrtc
