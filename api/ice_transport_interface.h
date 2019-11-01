/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ICE_TRANSPORT_INTERFACE_H_
#define API_ICE_TRANSPORT_INTERFACE_H_

#include <string>

#include "api/async_resolver_factory.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace cricket {
class IceTransportInternal;
class PortAllocator;
}  // namespace cricket

namespace webrtc {

// An ICE transport, as represented to the outside world.
// This object is refcounted, and is therefore alive until the
// last holder has released it.
class IceTransportInterface : public rtc::RefCountInterface {
 public:
  // Accessor for the internal representation of an ICE transport.
  // The returned object can only be safely used on the signalling thread.
  // TODO(crbug.com/907849): Add API calls for the functions that have to
  // be exposed to clients, and stop allowing access to the
  // cricket::IceTransportInternal API.
  virtual cricket::IceTransportInternal* internal() = 0;
};

struct IceTransportInit final {
 public:
  IceTransportInit() = default;
  IceTransportInit(const IceTransportInit&) = delete;
  IceTransportInit(IceTransportInit&&) = default;
  IceTransportInit& operator=(const IceTransportInit&) = delete;
  IceTransportInit& operator=(IceTransportInit&&) = default;

  cricket::PortAllocator* port_allocator() { return port_allocator_; }
  void set_port_allocator(cricket::PortAllocator* port_allocator) {
    port_allocator_ = port_allocator;
  }

  AsyncResolverFactory* async_resolver_factory() {
    return async_resolver_factory_;
  }
  void set_async_resolver_factory(
      AsyncResolverFactory* async_resolver_factory) {
    async_resolver_factory_ = async_resolver_factory;
  }

  RtcEventLog* event_log() { return event_log_; }
  void set_event_log(RtcEventLog* event_log) { event_log_ = event_log; }

 private:
  cricket::PortAllocator* port_allocator_ = nullptr;
  AsyncResolverFactory* async_resolver_factory_ = nullptr;
  RtcEventLog* event_log_ = nullptr;
};

class IceTransportFactory {
 public:
  virtual ~IceTransportFactory() = default;
  virtual rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      const IceTransportInit& init) = 0;
};

}  // namespace webrtc
#endif  // API_ICE_TRANSPORT_INTERFACE_H_
