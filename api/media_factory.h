/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIA_FACTORY_H_
#define API_MEDIA_FACTORY_H_

#include <memory>

#include "api/call/call_factory_interface.h"

// This header prefers forward declaration over includes when possible to reduce
// number of headers leaked to api.

namespace cricket {
class MediaEngineInterface;
}  // namespace cricket

namespace webrtc {

class Call;
struct CallConfig;
struct PeerConnectionFactoryDependencies;

// This interface allows webrtc to be optionally built without media support.
// See `PeerConnectionFactoryDependencies::media_factory` for more details.
// TODO(bugs.webrtc.org/15574): Delete CallFactoryInterface when call_factory
// is removed from PeerConnectionFactoryDependencies
class MediaFactory : public CallFactoryInterface {
 public:
  virtual ~MediaFactory() = default;

 private:
  // Usage of this interface is webrtc implementation details.
  friend class ConnectionContext;
  std::unique_ptr<Call> CreateCall(const CallConfig& config) override = 0;
  virtual std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& dependencies) = 0;
};

}  // namespace webrtc

#endif  // API_MEDIA_FACTORY_H_
