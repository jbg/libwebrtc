/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/usrsctp_wrapper.h"

#include <unordered_map>
#include <utility>

#include "absl/base/attributes.h"
#include "media/sctp/sctp_transport_internal.h"  // For some constants.
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"

namespace {

// Successful return value from usrsctp callbacks. Is not actually used by
// usrsctp, but all example programs for usrsctp use 1 as their return value.
constexpr int kSctpSuccessReturn = 1;
constexpr int kSctpErrorReturn = 0;

// Interval at which we process usrsctp timers. Note that 10 milliseconds is the
// same interval that would be used by the `SCTP timer` thread within usrsctp,
// if we were using |usrsctp_init| instead of |usrsctp_init_nothreads|.
static constexpr int kUsrsctpTimerGranularityMillis = 10;

ABSL_CONST_INIT bool g_usrsctp_initialized_ = false;
ABSL_CONST_INIT webrtc::GlobalMutex g_usrsctp_lock_(absl::kConstInit);

// Helper for logging SCTP messages.
#if defined(__GNUC__)
__attribute__((__format__(__printf__, 1, 2)))
#endif
void DebugSctpPrintf(const char* format, ...) {
#if RTC_DCHECK_IS_ON
  char s[255];
  va_list ap;
  va_start(ap, format);
  vsnprintf(s, sizeof(s), format, ap);
  RTC_LOG(LS_INFO) << "SCTP: " << s;
  va_end(ap);
#endif
}

// Log the packet in text2pcap format, if log level is at LS_VERBOSE.
//
// In order to turn these logs into a pcap file you can use, first filter the
// "SCTP_PACKET" log lines:
//
//   cat chrome_debug.log | grep SCTP_PACKET > filtered.log
//
// Then run through text2pcap:
//
//   text2pcap -n -l 248 -D -t '%H:%M:%S.' filtered.log filtered.pcapng
//
// Command flag information:
// -n: Outputs to a pcapng file, can specify inbound/outbound packets.
// -l: Specifies the link layer header type. 248 means SCTP. See:
//     http://www.tcpdump.org/linktypes.html
// -D: Text before packet specifies if it is inbound or outbound.
// -t: Time format.
//
// Why do all this? Because SCTP goes over DTLS, which is encrypted. So just
// getting a normal packet capture won't help you, unless you have the DTLS
// keying material.
void VerboseLogPacket(const void* data, size_t length, int direction) {
  if (RTC_LOG_CHECK_LEVEL(LS_VERBOSE) && length > 0) {
    char* dump_buf;
    // Some downstream project uses an older version of usrsctp that expects
    // a non-const "void*" as first parameter when dumping the packet, so we
    // need to cast the const away here to avoid a compiler error.
    if ((dump_buf = usrsctp_dumppacket(const_cast<void*>(data), length,
                                       direction)) != NULL) {
      RTC_LOG(LS_VERBOSE) << dump_buf;
      usrsctp_freedumpbuffer(dump_buf);
    }
  }
}

}  // namespace

namespace cricket {

ABSL_CONST_INIT UsrSctpWrapper* UsrSctpWrapper::instance_ = nullptr;

// Maps SCTP transport ID to SctpTransport object, necessary in send threshold
// callback and outgoing packet callback. It also provides a facility to
// safely post a task to an SctpTransport's network thread from another thread.
class UsrSctpWrapper::SctpTransportMap {
 public:
  SctpTransportMap() = default;

  // Assigns a new unused ID to the following transport.
  uintptr_t Register(UsrSctpWrapperDelegate* transport) {
    webrtc::MutexLock lock(&lock_);
    // usrsctp_connect fails with a value of 0...
    if (next_id_ == 0) {
      ++next_id_;
    }
    // In case we've wrapped around and need to find an empty spot from a
    // removed transport. Assumes we'll never be full.
    while (map_.find(next_id_) != map_.end()) {
      ++next_id_;
      if (next_id_ == 0) {
        ++next_id_;
      }
    }
    map_[next_id_] = transport;
    return next_id_++;
  }

  // Returns true if found.
  bool Deregister(uintptr_t id) {
    webrtc::MutexLock lock(&lock_);
    return map_.erase(id) > 0;
  }

  // Posts |action| to the network thread of the transport identified by |id|
  // and returns true if found, all while holding a lock to protect against the
  // transport being simultaneously deleted/deregistered, or returns false if
  // not found.
  template <typename F>
  bool PostToTransportThread(uintptr_t id, F action) const {
    webrtc::MutexLock lock(&lock_);
    auto it = map_.find(id);
    if (it == map_.end()) {
      return false;
    }
    UsrSctpWrapperDelegate* transport = it->second;
    if (!transport) {
      return false;
    }
    transport->network_thread()->PostTask(ToQueuedTask(
        transport->task_safety(),
        [transport, action{std::move(action)}]() { action(transport); }));
    return true;
  }

 private:
  // Note: In theory this lock wouldn't be necessary, as all operations on this
  // class should run on UsrSctpWrapper::thread_. But there is still a usrsctp
  // "interator thread" which could in theory invoke a callback, so we still use
  // a lock here to be safe
  mutable webrtc::Mutex lock_;

  uintptr_t next_id_ RTC_GUARDED_BY(lock_) = 0;
  std::unordered_map<uintptr_t, UsrSctpWrapperDelegate*> map_
      RTC_GUARDED_BY(lock_);
};

// static
rtc::scoped_refptr<UsrSctpWrapper> UsrSctpWrapper::GetOrCreateInstance(
    rtc::Thread* usrsctp_thread) {
  webrtc::GlobalMutexLock lock(&g_usrsctp_lock_);
  if (instance_) {
    if (!instance_->ValidateThread(usrsctp_thread)) {
      return nullptr;
    }
  } else {
    // This pointer will be cleared by Release.
    instance_ = new UsrSctpWrapper(usrsctp_thread);
  }
  return rtc::scoped_refptr<UsrSctpWrapper>(instance_);
}

void UsrSctpWrapper::AddRef() const {
  ref_count_.IncRef();
}

rtc::RefCountReleaseStatus UsrSctpWrapper::Release() const {
  webrtc::GlobalMutexLock lock(&g_usrsctp_lock_);
  const auto status = ref_count_.DecRef();
  if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
    delete this;
    instance_ = nullptr;
  }
  return status;
}

int UsrSctpWrapper::GetLAddrs(struct socket* so,
                              sctp_assoc_t id,
                              struct sockaddr** raddrs) {
  int errno_value;
  auto l = [so, id, raddrs, errno_out{&errno_value}] {
    auto ret = usrsctp_getladdrs(so, id, raddrs);
    *errno_out = errno;
    return ret;
  };
  int ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<int>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

void UsrSctpWrapper::FreeLAddrs(struct sockaddr* addrs) {
  int errno_value;
  auto l = [addrs, errno_out{&errno_value}] {
    usrsctp_freeladdrs(addrs);
    *errno_out = errno;
  };
  if (thread_->IsCurrent()) {
    l();
  } else {
    thread_->Invoke<void>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
}

ssize_t UsrSctpWrapper::SendV(struct socket* so,
                              const void* data,
                              size_t len,
                              struct sockaddr* to,
                              int addrcnt,
                              void* info,
                              socklen_t infolen,
                              unsigned int infotype,
                              int flags) {
  int errno_value;
  auto l = [so, data, len, to, addrcnt, info, infolen, infotype, flags,
            errno_out{&errno_value}] {
    auto ret = usrsctp_sendv(so, data, len, to, addrcnt, info, infolen,
                             infotype, flags);
    *errno_out = errno;
    return ret;
  };
  ssize_t ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<ssize_t>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

int UsrSctpWrapper::Bind(struct socket* so,
                         struct sockaddr* name,
                         int namelen) {
  int errno_value;
  auto l = [so, name, namelen, errno_out{&errno_value}] {
    auto ret = usrsctp_bind(so, name, namelen);
    *errno_out = errno;
    return ret;
  };
  int ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<int>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

int UsrSctpWrapper::Connect(struct socket* so,
                            struct sockaddr* name,
                            int namelen) {
  int errno_value;
  auto l = [so, name, namelen, errno_out{&errno_value}] {
    auto ret = usrsctp_connect(so, name, namelen);
    *errno_out = errno;
    return ret;
  };
  int ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<int>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

int UsrSctpWrapper::SetSockOpt(struct socket* so,
                               int level,
                               int option_name,
                               const void* option_value,
                               socklen_t option_len) {
  int errno_value;
  auto l = [so, level, option_name, option_value, option_len,
            errno_out{&errno_value}] {
    auto ret =
        usrsctp_setsockopt(so, level, option_name, option_value, option_len);
    *errno_out = errno;
    return ret;
  };
  int ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<int>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

struct socket* UsrSctpWrapper::Socket(int domain,
                                      int type,
                                      int protocol,
                                      uint32_t sb_threshold,
                                      UsrSctpWrapperDelegate* transport) {
  int errno_value;
  auto l = [domain, type, protocol, sb_threshold, transport,
            errno_out{&errno_value}] {
    auto ret = usrsctp_socket(
        domain, type, protocol, &UsrSctpWrapper::OnSctpInboundPacket,
        &UsrSctpWrapper::SendThresholdCallback, sb_threshold, transport);
    *errno_out = errno;
    return ret;
  };
  struct socket* ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<struct socket*>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

uint32_t UsrSctpWrapper::GetSctpSendspace() {
  int errno_value;
  auto l = [errno_out{&errno_value}] {
    auto ret = usrsctp_sysctl_get_sctp_sendspace();
    *errno_out = errno;
    return ret;
  };
  uint32_t ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<uint32_t>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

uintptr_t UsrSctpWrapper::Register(UsrSctpWrapperDelegate* transport) {
  int errno_value;
  auto l = [this, transport, errno_out{&errno_value}] {
    uintptr_t id = transport_map_->Register(transport);
    usrsctp_register_address(reinterpret_cast<void*>(id));
    *errno_out = errno;
    return id;
  };
  uintptr_t ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<uintptr_t>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

bool UsrSctpWrapper::Deregister(uintptr_t id) {
  int errno_value;
  auto l = [this, id, errno_out{&errno_value}] {
    usrsctp_deregister_address(reinterpret_cast<void*>(id));
    *errno_out = errno;
    return transport_map_->Deregister(id);
  };
  bool ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<bool>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

void UsrSctpWrapper::ConnInput(void* addr,
                               const void* buffer,
                               size_t length,
                               uint8_t ecn_bits) {
  int errno_value;
  auto l = [addr, buffer, length, ecn_bits, errno_out{&errno_value}] {
    VerboseLogPacket(buffer, length, SCTP_DUMP_INBOUND);
    usrsctp_conninput(addr, buffer, length, ecn_bits);
    *errno_out = errno;
  };
  if (thread_->IsCurrent()) {
    l();
  } else {
    thread_->Invoke<void>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
}

void UsrSctpWrapper::Close(struct socket* so) {
  int errno_value;
  auto l = [so, errno_out{&errno_value}] {
    usrsctp_close(so);
    *errno_out = errno;
  };
  if (thread_->IsCurrent()) {
    l();
  } else {
    thread_->Invoke<void>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
}

int UsrSctpWrapper::SetNonBlocking(struct socket* so, int onoff) {
  int errno_value;
  auto l = [so, onoff, errno_out{&errno_value}] {
    int ret = usrsctp_set_non_blocking(so, onoff);
    *errno_out = errno;
    return ret;
  };
  int ret;
  if (thread_->IsCurrent()) {
    ret = l();
  } else {
    ret = thread_->Invoke<int>(RTC_FROM_HERE, l);
  }
  errno = errno_value;
  return ret;
}

UsrSctpWrapper::UsrSctpWrapper(rtc::Thread* usrsctp_thread)
    : transport_map_(new SctpTransportMap()) {
  if (usrsctp_thread) {
    thread_ = usrsctp_thread;
  } else {
    owned_thread_ = rtc::Thread::Create();
    owned_thread_->Start();
    thread_ = owned_thread_.get();
  }
  auto l = [this] {
    InitializeUsrSctp();
    last_handled_timers_ms_ = rtc::TimeMillis();
    timer_task_handle_ = webrtc::RepeatingTaskHandle::DelayedStart(
        thread_, webrtc::TimeDelta::Millis(kUsrsctpTimerGranularityMillis),
        [this] { return HandleTimers(); });
  };
  if (thread_->IsCurrent()) {
    l();
  } else {
    thread_->Invoke<void>(RTC_FROM_HERE, l);
  }
}

UsrSctpWrapper::~UsrSctpWrapper() {
  auto l = [this] {
    timer_task_handle_.Stop();
    DeinitializeUsrSctp();
  };
  if (thread_->IsCurrent()) {
    l();
  } else {
    thread_->Invoke<void>(RTC_FROM_HERE, l);
  }
}

bool UsrSctpWrapper::ValidateThread(rtc::Thread* usrsctp_thread) const {
  rtc::Thread* initial_thread = owned_thread_ ? nullptr : thread_;
  if (usrsctp_thread != initial_thread) {
    RTC_LOG(LS_ERROR) << "usrsctp being initialized with thread "
                      << usrsctp_thread
                      << " after initially being initialized with thread "
                      << initial_thread;
    return false;
  }
  return true;
}

void UsrSctpWrapper::InitializeUsrSctp() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  // UninitializeUsrSctp tries to call usrsctp_finish in a loop for three
  // seconds; if that failed and we were left in a still-initialized state, we
  // don't want to call usrsctp_init again as that will result in undefined
  // behavior.
  if (g_usrsctp_initialized_) {
    RTC_LOG(LS_WARNING) << "Not reinitializing usrsctp since last attempt at "
                           "usrsctp_finish failed.";
  } else {
    // First argument is udp_encapsulation_port, which is not releveant for
    // our AF_CONN use of sctp.
    //
    // Note that this still spawns an extra "interator thread" which is used
    // for operations that need to iterate over all associations, so we still
    // need to be prepared for callbacks occurring on a thread other than
    // |thread_|.
    usrsctp_init_nothreads(0, &UsrSctpWrapper::OnSctpOutboundPacket,
                           &DebugSctpPrintf);
    g_usrsctp_initialized_ = true;
  }

  // To turn on/off detailed SCTP debugging. You will also need to have the
  // SCTP_DEBUG cpp defines flag, which can be turned on in media/BUILD.gn.
  // usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);

  // TODO(ldixon): Consider turning this on/off.
  usrsctp_sysctl_set_sctp_ecn_enable(0);

  // WebRTC doesn't use these features, so disable them to reduce the
  // potential attack surface.
  usrsctp_sysctl_set_sctp_asconf_enable(0);
  usrsctp_sysctl_set_sctp_auth_enable(0);

  // This is harmless, but we should find out when the library default
  // changes.
  int send_size = usrsctp_sysctl_get_sctp_sendspace();
  if (send_size != kSctpSendBufferSize) {
    RTC_LOG(LS_ERROR) << "Got different send size than expected: " << send_size;
  }

  // TODO(ldixon): Consider turning this on/off.
  // This is not needed right now (we don't do dynamic address changes):
  // If SCTP Auto-ASCONF is enabled, the peer is informed automatically
  // when a new address is added or removed. This feature is enabled by
  // default.
  // usrsctp_sysctl_set_sctp_auto_asconf(0);

  // TODO(ldixon): Consider turning this on/off.
  // Add a blackhole sysctl. Setting it to 1 results in no ABORTs
  // being sent in response to INITs, setting it to 2 results
  // in no ABORTs being sent for received OOTB packets.
  // This is similar to the TCP sysctl.
  //
  // See: http://lakerest.net/pipermail/sctp-coders/2012-January/009438.html
  // See: http://svnweb.freebsd.org/base?view=revision&revision=229805
  // usrsctp_sysctl_set_sctp_blackhole(2);

  // Set the number of default outgoing streams. This is the number we'll
  // send in the SCTP INIT message.
  usrsctp_sysctl_set_sctp_nr_outgoing_streams_default(kMaxSctpStreams);
}

void UsrSctpWrapper::DeinitializeUsrSctp() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  // Even though we initialized in single threaded mode, there is still an
  // "iterator thread" which may be doing some work that prevents
  // usrsctp_finish from completing. Wait and try again until it succeeds for
  // up to 3 seconds; if it doesn't succeed it's likely due to a reference
  // counting bug.
  for (size_t i = 0; i < 300; ++i) {
    if (usrsctp_finish() == 0) {
      g_usrsctp_initialized_ = false;
      return;
    }
    rtc::Thread::SleepMs(10);
  }
  RTC_LOG(LS_ERROR) << "Failed to shutdown usrsctp.";
}

webrtc::TimeDelta UsrSctpWrapper::HandleTimers() {
  RTC_DCHECK_RUN_ON(thread_);
  auto now = rtc::TimeMillis();
  auto elapsed_millis = now - last_handled_timers_ms_;
  last_handled_timers_ms_ = now;
  usrsctp_handle_timers(elapsed_millis);
  return webrtc::TimeDelta::Millis(kUsrsctpTimerGranularityMillis);
}

// static
absl::optional<uintptr_t> UsrSctpWrapper::GetTransportIdFromSocket(
    struct socket* sock) {
  absl::optional<uintptr_t> ret;
  struct sockaddr* addrs = nullptr;
  int naddrs = usrsctp_getladdrs(sock, 0, &addrs);
  if (naddrs <= 0 || addrs[0].sa_family != AF_CONN) {
    return ret;
  }
  // usrsctp_getladdrs() returns the addresses bound to this socket, which
  // contains the SctpTransport id as sconn_addr.  Read the id,
  // then free the list of addresses once we have the pointer.  We only open
  // AF_CONN sockets, and they should all have the sconn_addr set to the
  // id of the transport that created them, so [0] is as good as any other.
  struct sockaddr_conn* sconn =
      reinterpret_cast<struct sockaddr_conn*>(&addrs[0]);
  ret = reinterpret_cast<uintptr_t>(sconn->sconn_addr);
  usrsctp_freeladdrs(addrs);

  return ret;
}

// static
int UsrSctpWrapper::OnSctpOutboundPacket(void* addr,
                                         void* data,
                                         size_t length,
                                         uint8_t tos,
                                         uint8_t set_df) {
  if (!instance_) {
    RTC_LOG(LS_ERROR)
        << "OnSctpOutboundPacket called after usrsctp uninitialized?";
    return EINVAL;
  }
  // Even though usrsctp has an "iterator thread" that's still running in
  // single-threaded mode, we don't expect it to invoke any callbacks.
  RTC_DCHECK_RUN_ON(instance_->thread_);

  RTC_LOG(LS_VERBOSE) << "global OnSctpOutboundPacket():"
                         "addr: "
                      << addr << "; length: " << length
                      << "; tos: " << rtc::ToHex(tos)
                      << "; set_df: " << rtc::ToHex(set_df);

  VerboseLogPacket(data, length, SCTP_DUMP_OUTBOUND);

  // Note: We have to copy the data; the caller will delete it.
  rtc::CopyOnWriteBuffer buf(reinterpret_cast<uint8_t*>(data), length);

  // PostsToTransportThread protects against the transport being
  // simultaneously deregistered/deleted, since this callback may come from
  // the SCTP timer thread and thus race with the network thread.
  bool found = instance_->transport_map_->PostToTransportThread(
      reinterpret_cast<uintptr_t>(addr),
      [buf](UsrSctpWrapperDelegate* transport) {
        transport->OnPacketFromSctpToNetwork(buf);
      });
  if (!found) {
    RTC_LOG(LS_ERROR)
        << "OnSctpOutboundPacket: Failed to get transport for socket ID "
        << addr << "; possibly was already destroyed.";
    return EINVAL;
  }

  return 0;
}

// static
int UsrSctpWrapper::OnSctpInboundPacket(struct socket* sock,
                                        union sctp_sockstore addr,
                                        void* data,
                                        size_t length,
                                        struct sctp_rcvinfo rcv,
                                        int flags,
                                        void* ulp_info) {
  struct DeleteByFree {
    void operator()(void* p) const { free(p); }
  };
  std::unique_ptr<void, DeleteByFree> owned_data(data, DeleteByFree());

  absl::optional<uintptr_t> id = GetTransportIdFromSocket(sock);
  if (!id) {
    RTC_LOG(LS_ERROR)
        << "OnSctpInboundPacket: Failed to get transport ID from socket "
        << sock;
    return kSctpErrorReturn;
  }

  if (!instance_) {
    RTC_LOG(LS_ERROR)
        << "OnSctpInboundPacket called after usrsctp uninitialized?";
    return kSctpErrorReturn;
  }
  // Even though usrsctp has an "iterator thread" that's still running in
  // single-threaded mode, we don't expect it to invoke any callbacks.
  RTC_DCHECK_RUN_ON(instance_->thread_);

  // PostsToTransportThread protects against the transport being
  // simultaneously deregistered/deleted, since this callback may come from
  // the SCTP timer thread and thus race with the network thread.
  bool found = instance_->transport_map_->PostToTransportThread(
      *id, [owned_data{std::move(owned_data)}, length, rcv,
            flags](UsrSctpWrapperDelegate* transport) {
        transport->OnDataOrNotificationFromSctp(owned_data.get(), length, rcv,
                                                flags);
      });
  if (!found) {
    RTC_LOG(LS_ERROR)
        << "OnSctpInboundPacket: Failed to get transport for socket ID " << *id
        << "; possibly was already destroyed.";
    return kSctpErrorReturn;
  }
  return kSctpSuccessReturn;
}

// static
int UsrSctpWrapper::SendThresholdCallback(struct socket* sock,
                                          uint32_t sb_free,
                                          void* ulp_info) {
  // Fired on our I/O thread. SctpTransport::OnPacketReceived() gets
  // a packet containing acknowledgments, which goes into usrsctp_conninput,
  // and then back here.
  absl::optional<uintptr_t> id = GetTransportIdFromSocket(sock);
  if (!id) {
    RTC_LOG(LS_ERROR)
        << "SendThresholdCallback: Failed to get transport ID from socket "
        << sock;
    return 0;
  }
  if (!instance_) {
    RTC_LOG(LS_ERROR)
        << "SendThresholdCallback called after usrsctp uninitialized?";
    return 0;
  }
  // Even though usrsctp has an "iterator thread" that's still running in
  // single-threaded mode, we don't expect it to invoke any callbacks.
  RTC_DCHECK_RUN_ON(instance_->thread_);
  bool found = instance_->transport_map_->PostToTransportThread(
      *id, [](UsrSctpWrapperDelegate* transport) {
        transport->OnSendThresholdCallback();
      });
  if (!found) {
    RTC_LOG(LS_ERROR)
        << "SendThresholdCallback: Failed to get transport for socket ID "
        << *id << "; possibly was already destroyed.";
  }
  return 0;
}

}  // namespace cricket
