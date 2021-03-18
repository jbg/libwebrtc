/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_SCTP_USRSCTP_WRAPPER_H_
#define MEDIA_SCTP_USRSCTP_WRAPPER_H_

#include <usrsctp.h>

#include <cstdint>
#include <memory>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread.h"

namespace cricket {

// Used to avoid circular dependency between UsrSctpWrapper and SctpTransport.
class UsrSctpWrapperDelegate {
 public:
  virtual ~UsrSctpWrapperDelegate() {}
  virtual const webrtc::ScopedTaskSafety& task_safety() const = 0;
  virtual rtc::Thread* network_thread() const = 0;

  virtual void OnSendThresholdCallback() = 0;
  virtual void OnPacketFromSctpToNetwork(
      const rtc::CopyOnWriteBuffer& buffer) = 0;
  virtual void OnDataOrNotificationFromSctp(const void* data,
                                            size_t length,
                                            struct sctp_rcvinfo rcv,
                                            int flags) = 0;
};

// Represents instance of usrsctp library initialized in single-threaded mode.
//
// Handles global init/deinit, and mapping from usrsctp callbacks to
// SctpTransport calls. Also provides wrapper methods for usrsctp functions used
// by SctpTransport to marshal method execution to appropriate thread.
class UsrSctpWrapper : public rtc::RefCountInterface {
 public:
  static rtc::scoped_refptr<UsrSctpWrapper> GetOrCreateInstance(
      rtc::Thread* usrsctp_thread);

  ~UsrSctpWrapper();

  // RefCountedObject implementation.
  void AddRef() const override;
  rtc::RefCountReleaseStatus Release() const override;

  // All of these methods invoke the corresponding usrsctp method (for example,
  // usrsctp_getladdrs) on |thread_| and return the result, also copying the
  // value of errno.
  int GetLAddrs(struct socket* so, sctp_assoc_t id, struct sockaddr** raddrs);
  void FreeLAddrs(struct sockaddr* addrs);
  ssize_t SendV(struct socket* so,
                const void* data,
                size_t len,
                struct sockaddr* to,
                int addrcnt,
                void* info,
                socklen_t infolen,
                unsigned int infotype,
                int flags);
  int Bind(struct socket* so, struct sockaddr* name, int namelen);
  int Connect(struct socket* so, struct sockaddr* name, int namelen);
  int SetSockOpt(struct socket* so,
                 int level,
                 int option_name,
                 const void* option_value,
                 socklen_t option_len);
  struct socket* Socket(int domain,
                        int type,
                        int protocol,
                        uint32_t sb_threshold,
                        UsrSctpWrapperDelegate* transport);
  uint32_t GetSctpSendspace();
  uintptr_t Register(UsrSctpWrapperDelegate* transport);
  bool Deregister(uintptr_t id);
  void ConnInput(void* addr,
                 const void* buffer,
                 size_t length,
                 uint8_t ecn_bits);
  void Close(struct socket* so);
  int SetNonBlocking(struct socket* so, int onoff);

 private:
  class SctpTransportMap;

  explicit UsrSctpWrapper(rtc::Thread* usrsctp_thread);

  // Ensures that UsrSctpWrapper is being initialized with the same thread as it
  // was previously.
  bool ValidateThread(rtc::Thread* usrsctp_thread) const;

  void InitializeUsrSctp();
  void DeinitializeUsrSctp();

  webrtc::TimeDelta HandleTimers();

  static absl::optional<uintptr_t> GetTransportIdFromSocket(
      struct socket* sock);

  // This is the callback usrsctp uses when there's data to send on the network
  // that has been wrapped appropriatly for the SCTP protocol.
  static int OnSctpOutboundPacket(void* addr,
                                  void* data,
                                  size_t length,
                                  uint8_t tos,
                                  uint8_t set_df);

  // This is the callback called from usrsctp when data has been received, after
  // a packet has been interpreted and parsed by usrsctp and found to contain
  // payload data. It is called by a usrsctp thread. It is assumed this function
  // will free the memory used by 'data'.
  static int OnSctpInboundPacket(struct socket* sock,
                                 union sctp_sockstore addr,
                                 void* data,
                                 size_t length,
                                 struct sctp_rcvinfo rcv,
                                 int flags,
                                 void* ulp_info);

  // TODO(crbug.com/webrtc/11899): This is a legacy callback signature, remove
  // when usrsctp is updated.
  static int SendThresholdCallback(struct socket* sock, uint32_t sb_free);

  static int SendThresholdCallback(struct socket* sock,
                                   uint32_t sb_free,
                                   void* ulp_info);

  static UsrSctpWrapper* instance_;

  rtc::Thread* thread_;
  std::unique_ptr<rtc::Thread> owned_thread_;

  std::unique_ptr<SctpTransportMap> transport_map_;

  webrtc::RepeatingTaskHandle timer_task_handle_;
  int64_t last_handled_timers_ms_;

  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}  // namespace cricket

#endif  // MEDIA_SCTP_USRSCTP_WRAPPER_H_
