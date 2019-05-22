/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_MEDIA_TRANSPORT_CONFIG_H_
#define API_MEDIA_TRANSPORT_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"

namespace webrtc {

class MediaTransportInterface;
class DatagramTransportInterface;

// Media transport config is made available to both transport and audio / video
// layers, but access to individual interfaces should not be open without
// necessity.
struct MediaTransportConfig {
  // Default constructor for no-media transport scenarios.
  MediaTransportConfig() = default;

  // TODO(sukhanov): Consider adding RtpTransport* to MediaTransportConfig,
  // because it's almost always passes along with media_transport.
  //
  // Note that |media_transport| and |rtp_max_packet_size| can not be set
  // together (because |rtp_max_packet_size| is only used with datagram
  // transport).
  MediaTransportConfig(MediaTransportInterface* media_transport,
                       absl::optional<size_t> rtp_max_packet_size);

  // Constructor for media_transport scenarios.
  // Note that |media_transport| may not be nullptr.
  explicit MediaTransportConfig(MediaTransportInterface* media_transport);

  std::string DebugString() const;

  // If provided, all media is sent through media_transport.
  MediaTransportInterface* media_transport = nullptr;

  // If provided, limits RTP packet size (excludes ICE, IP or network overhead).
  absl::optional<size_t> rtp_max_packet_size;
};

}  // namespace webrtc

#endif  // API_MEDIA_TRANSPORT_CONFIG_H_
