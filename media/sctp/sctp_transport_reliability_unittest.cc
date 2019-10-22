/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/sctp/sctp_transport.h"

#include <memory>
#include <queue>
#include <string>

#include "media/sctp/sctp_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace {

static constexpr int kDefaultTimeout = 10000;  // 10 seconds.
static constexpr int kTransport1Port = 15001;
static constexpr int kTransport2Port = 25002;
static constexpr int kLogPerMessagesCount = 100;

class LossyPacketTransport final : public rtc::PacketTransportInternal {
 public:
  LossyPacketTransport(std::string name, rtc::Thread* transport_thread)
      : transport_name_(name),
        transport_thread_(transport_thread),
        random_(42) {
    RTC_DCHECK(transport_thread_);
    RTC_DCHECK_RUN_ON(transport_thread_);
  }

  ~LossyPacketTransport() {
    RTC_DCHECK_RUN_ON(transport_thread_);
    if (destination_ != nullptr) {
      invoker_.Flush(destination_->transport_thread_);
    }
    invoker_.Flush(transport_thread_);
    destination_ = nullptr;
    SignalWritableState(this);
  }

  const std::string& transport_name() const override { return transport_name_; }

  bool writable() const override { return destination_ != nullptr; }

  bool receiving() const override { return true; }

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags = 0) {
    RTC_DCHECK_RUN_ON(transport_thread_);
    if (destination_ == nullptr) {
      return -1;
    }
    if (random_.Rand(100) < packet_loss_percents_) {
      // silent packet loss
      return 0;
    }
    rtc::CopyOnWriteBuffer buffer(data, len);
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, destination_->transport_thread_,
        [this, flags, buffer = std::move(buffer)] {
          if (destination_ == nullptr) {
            return;
          }
          destination_->SignalReadPacket(
              destination_, reinterpret_cast<const char*>(buffer.data()),
              buffer.size(), rtc::Time(), flags);
        });
    return 0;
  }

  int SetOption(rtc::Socket::Option opt, int value) override { return 0; }

  bool GetOption(rtc::Socket::Option opt, int* value) override { return false; }

  int GetError() override { return 0; }

  absl::optional<rtc::NetworkRoute> network_route() const override {
    return absl::nullopt;
  }

  void SetDestination(LossyPacketTransport* destination) {
    RTC_DCHECK_RUN_ON(transport_thread_);
    if (destination == this) {
      return;
    }
    destination_ = destination;
    SignalWritableState(this);
  }

  void SetPacketLossRate(uint16_t packet_loss_percents) {
    RTC_DCHECK_RUN_ON(transport_thread_);
    if (packet_loss_percents > 100) {
      packet_loss_percents = 100;
    }
    packet_loss_percents_ = packet_loss_percents;
  }

 private:
  std::string transport_name_;
  uint16_t packet_loss_percents_ = 0;
  LossyPacketTransport* destination_;
  rtc::Thread* transport_thread_;
  rtc::AsyncInvoker invoker_;
  webrtc::Random random_;
  RTC_DISALLOW_COPY_AND_ASSIGN(LossyPacketTransport);
};

class SctpDataSender final {
 public:
  SctpDataSender(rtc::Thread* thread,
                 cricket::SctpTransport* transport,
                 uint64_t target_messages_count,
                 cricket::SendDataParams send_params,
                 uint32_t sender_id)
      : thread_(thread),
        transport_(transport),
        target_messages_count_(target_messages_count),
        send_params_(send_params),
        sender_id_(sender_id) {
    RTC_DCHECK(thread_);
    RTC_DCHECK(transport_);
  }

  ~SctpDataSender() {
    if (started_) {
      started_ = false;
      WaitForCompletion(rtc::Event::kForever);
    }
  }

  void Start() {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
      if (started_) {
        RTC_LOG(LS_INFO) << sender_id_ << " sender is already started";
        return;
      }
      started_ = true;
      SendNextMessage();
    });
  }

  uint64_t BytesSentCount() const { return num_bytes_sent_; }

  uint64_t MessagesSentCount() const { return num_messages_sent_; }

  absl::optional<std::string> GetLastError() {
    absl::optional<std::string> result = absl::nullopt;
    thread_->Invoke<void>(RTC_FROM_HERE,
                          [this, &result] { result = last_error_; });
    return result;
  }

  bool WaitForCompletion(int give_up_after_ms) {
    return completed_.Wait(give_up_after_ms, kDefaultTimeout);
  }

 private:
  void SendNextMessage() {
    RTC_DCHECK_RUN_ON(thread_);
    if (!started_ || num_messages_sent_ >= target_messages_count_) {
      completed_.Set();
      return;
    }

    if (num_messages_sent_ % kLogPerMessagesCount == 0) {
      RTC_LOG(LS_INFO) << sender_id_ << " sender will try send message "
                       << (num_messages_sent_ + 1) << " out of "
                       << target_messages_count_;
    }

    cricket::SendDataParams params(send_params_);
    if (params.sid < 0) {
      params.sid = cricket::kMinSctpSid +
                   (num_messages_sent_ % cricket::kMaxSctpStreams);
    }

    cricket::SendDataResult result;
    transport_->SendData(params, payload_, &result);
    switch (result) {
      case cricket::SDR_BLOCK:
        // retry after timeout
        invoker_.AsyncInvokeDelayed<void>(
            RTC_FROM_HERE, thread_,
            rtc::Bind(&SctpDataSender::SendNextMessage, this), 500);
        break;
      case cricket::SDR_SUCCESS:
        // send next
        num_bytes_sent_ += payload_.size();
        ++num_messages_sent_;
        invoker_.AsyncInvoke<void>(
            RTC_FROM_HERE, thread_,
            rtc::Bind(&SctpDataSender::SendNextMessage, this));
        break;
      case cricket::SDR_ERROR:
        // give up
        last_error_ = "SctpTransport::SendData error returned";
        completed_.Set();
        break;
    }
  }

  rtc::Thread* const thread_;
  cricket::SctpTransport* const transport_;
  const uint64_t target_messages_count_;
  const cricket::SendDataParams send_params_;
  const uint32_t sender_id_;
  rtc::CopyOnWriteBuffer payload_{std::string(1400, '.').c_str(), 1400};
  std::atomic<bool> started_ ATOMIC_VAR_INIT(false);
  rtc::AsyncInvoker invoker_;
  std::atomic<uint64_t> num_messages_sent_ ATOMIC_VAR_INIT(0);
  rtc::Event completed_{true, false};
  std::atomic<uint64_t> num_bytes_sent_ ATOMIC_VAR_INIT(0);
  absl::optional<std::string> last_error_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpDataSender);
};

class SctpDataReceiver final : public sigslot::has_slots<> {
 public:
  explicit SctpDataReceiver(uint32_t receiver_id,
                            uint64_t target_messages_count)
      : receiver_id_(receiver_id),
        target_messages_count_(target_messages_count) {}

  void OnDataReceived(const cricket::ReceiveDataParams& params,
                      const rtc::CopyOnWriteBuffer& data) {
    num_bytes_received_ += data.size();
    if (++num_messages_received_ == target_messages_count_) {
      received_target_messages_count_.Set();
    }

    if (num_messages_received_ % kLogPerMessagesCount == 0) {
      RTC_LOG(INFO) << receiver_id_ << " receiver got "
                    << num_messages_received_ << " messages";
    }
  }

  uint64_t MessagesReceivedCount() const { return num_messages_received_; }

  uint64_t BytesReceivedCount() const { return num_bytes_received_; }

  bool WaitForMessagesReceived(int timeout_millis) {
    return received_target_messages_count_.Wait(timeout_millis);
  }

 private:
  std::atomic<uint64_t> num_messages_received_ ATOMIC_VAR_INIT(0);
  std::atomic<uint64_t> num_bytes_received_ ATOMIC_VAR_INIT(0);
  rtc::Event received_target_messages_count_{true, false};
  const uint32_t receiver_id_;
  const uint64_t target_messages_count_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpDataReceiver);
};

class ThreadPool final {
 public:
  explicit ThreadPool(size_t threads_count) : random_(42) {
    RTC_DCHECK(threads_count > 0);
    threads_.reserve(threads_count);
    for (size_t i = 0; i < threads_count; i++) {
      auto thread = rtc::Thread::Create();
      thread->SetName("Thread #" + rtc::ToString(i + 1) + " from Pool", this);
      thread->Start();
      threads_.emplace_back(std::move(thread));
    }
  }

  rtc::Thread* GetRandomThread() {
    return threads_[random_.Rand(0, threads_.size() - 1)].get();
  }

 private:
  webrtc::Random random_;
  std::vector<std::unique_ptr<rtc::Thread>> threads_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

// SCTP reliability testing.
class SctpPingPong final {
 public:
  SctpPingPong(uint32_t id,
               uint16_t port1,
               uint16_t port2,
               rtc::Thread* transport_thread1,
               rtc::Thread* transport_thread2,
               uint32_t messages_count,
               uint16_t packet_loss_percents,
               cricket::SendDataParams send_params)
      : id_(id),
        port1_(port1),
        port2_(port2),
        transport_thread1_(transport_thread1),
        transport_thread2_(transport_thread2),
        messages_count_(messages_count),
        packet_loss_percents_(packet_loss_percents),
        send_params_(send_params) {
    RTC_DCHECK(transport_thread1_ != nullptr);
    RTC_DCHECK(transport_thread2_ != nullptr);
  }

  virtual ~SctpPingPong() {
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      sender1_.reset();
      sctp_transport1_->SetDtlsTransport(nullptr);
      packet_transport1_->SetDestination(nullptr);
    });
    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      sender2_.reset();
      sctp_transport2_->SetDtlsTransport(nullptr);
      packet_transport2_->SetDestination(nullptr);
    });
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      sctp_transport1_.reset();
      data_receiver1_.reset();
      packet_transport1_.reset();
    });
    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      sctp_transport2_.reset();
      data_receiver2_.reset();
      packet_transport2_.reset();
    });
  }

  bool Start() {
    CreateTwoConnectedSctpTransportsWithAllStreams();

    {
      rtc::CritScope cs(&lock_);
      if (!errors_list_.empty()) {
        return false;
      }
    }

    sender1_.reset(new SctpDataSender(transport_thread1(), sctp_transport1(),
                                      messages_count_, send_params_, id_));
    sender2_.reset(new SctpDataSender(transport_thread2(), sctp_transport2(),
                                      messages_count_, send_params_, id_));
    sender1_->Start();
    sender2_->Start();
    return true;
  }

  std::vector<std::string> GetErrorsList() const {
    std::vector<std::string> result;
    {
      rtc::CritScope cs(&lock_);
      result = errors_list_;
    }
    return result;
  }

  void WaitForCompletion(uint32_t timeout_millis) {
    if (data_sender1() == nullptr) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 is not created");
      return;
    }
    if (data_sender2() == nullptr) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 is not created");
      return;
    }

    if (!data_sender1()->WaitForCompletion(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 failed to complete within " +
                  rtc::ToString(timeout_millis) + " millis");
      return;
    }

    auto sender1_error = data_sender1()->GetLastError();
    if (sender1_error.has_value()) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 error: " + sender1_error.value());
      return;
    }

    if (!data_sender2()->WaitForCompletion(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 failed to complete within " +
                  rtc::ToString(timeout_millis) + " millis");
      return;
    }

    auto sender2_error = data_sender2()->GetLastError();
    if (sender2_error.has_value()) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 error: " + sender1_error.value());
      return;
    }

    if ((data_sender1()->MessagesSentCount() != messages_count_)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 sent only " +
                  rtc::ToString(sender1_->MessagesSentCount()) + " out of " +
                  rtc::ToString(messages_count_));
      return;
    }

    if ((data_sender2()->MessagesSentCount() != messages_count_)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 sent only " +
                  rtc::ToString(sender2_->MessagesSentCount()) + " out of " +
                  rtc::ToString(messages_count_));
      return;
    }

    if (!data_receiver1()->WaitForMessagesReceived(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", receiver 1 did not complete within " +
                  rtc::ToString(messages_count_));
      return;
    }

    if (!data_receiver2()->WaitForMessagesReceived(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", receiver 2 did not complete within " +
                  rtc::ToString(messages_count_));
      return;
    }

    if (data_receiver1()->BytesReceivedCount() !=
        data_sender2()->BytesSentCount()) {
      ReportError(
          "SctpPingPong id = " + rtc::ToString(id_) + ", receiver 1 received " +
          rtc::ToString(data_receiver1()->BytesReceivedCount()) +
          " bytes, but sender 2 send " +
          rtc::ToString(rtc::ToString(data_sender2()->BytesSentCount())));
      return;
    }

    if (data_receiver2()->BytesReceivedCount() !=
        data_sender1()->BytesSentCount()) {
      ReportError(
          "SctpPingPong id = " + rtc::ToString(id_) + ", receiver 2 received " +
          rtc::ToString(data_receiver2()->BytesReceivedCount()) +
          " bytes, but sender 1 send " +
          rtc::ToString(rtc::ToString(data_sender1()->BytesSentCount())));
      return;
    }

    RTC_LOG(LS_INFO) << "SctpPingPong id = " << id_ << " is done";
  }

 protected:
  void CreateTwoConnectedSctpTransportsWithAllStreams() {
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport1_.reset(
          new LossyPacketTransport("SctpPingPong id = " + rtc::ToString(id_) +
                                       ", lossy packet transport 1",
                                   transport_thread1_));
      data_receiver1_.reset(new SctpDataReceiver(id_, messages_count_));
      sctp_transport1_.reset(new cricket::SctpTransport(
          transport_thread1_, packet_transport1_.get()));
      sctp_transport1_->set_debug_name_for_testing("sctp transport 1");

      sctp_transport1_->SignalDataReceived.connect(
          data_receiver1_.get(), &SctpDataReceiver::OnDataReceived);

      for (uint32_t i = cricket::kMinSctpSid; i <= cricket::kMaxSctpSid; i++) {
        if (!sctp_transport1_->OpenStream(i)) {
          ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                      ", sctp transport 1 stream " + rtc::ToString(i) +
                      " failed to open");
          break;
        }
      }
    });

    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport2_.reset(new LossyPacketTransport(
          "SctpPingPong id = " + rtc::ToString(id_) + "packet transport 2",
          transport_thread2_));
      data_receiver2_.reset(new SctpDataReceiver(id_, messages_count_));
      sctp_transport2_.reset(new cricket::SctpTransport(
          transport_thread2_, packet_transport2_.get()));
      sctp_transport2_->set_debug_name_for_testing("sctp transport 2");
      sctp_transport2_->SignalDataReceived.connect(
          data_receiver2_.get(), &SctpDataReceiver::OnDataReceived);

      for (uint32_t i = cricket::kMinSctpSid; i <= cricket::kMaxSctpSid; i++) {
        if (!sctp_transport2_->OpenStream(i)) {
          ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                      ", sctp transport 2 stream " + rtc::ToString(i) +
                      " failed to open");
          break;
        }
      }
    });

    transport_thread1()->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport1()->SetDestination(packet_transport2());
      packet_transport1()->SetPacketLossRate(packet_loss_percents_);
    });
    transport_thread2()->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport2()->SetDestination(packet_transport1());
      packet_transport2()->SetPacketLossRate(packet_loss_percents_);
    });

    transport_thread1()->Invoke<void>(RTC_FROM_HERE, [this] {
      if (!sctp_transport1_->Start(port1_, port2_,
                                   cricket::kSctpSendBufferSize)) {
        ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                    ", failed to start sctp transport 1");
      }
    });

    transport_thread2()->Invoke<void>(RTC_FROM_HERE, [this] {
      if (!sctp_transport2_->Start(port2_, port1_,
                                   cricket::kSctpSendBufferSize)) {
        ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                    ", failed to start sctp transport 2");
      }
    });
  }

  rtc::Thread* transport_thread1() { return transport_thread1_; }

  rtc::Thread* transport_thread2() { return transport_thread2_; }

  SctpDataSender* data_sender1() { return sender1_.get(); }

  SctpDataSender* data_sender2() { return sender2_.get(); }

  SctpDataReceiver* data_receiver1() { return data_receiver1_.get(); }

  SctpDataReceiver* data_receiver2() { return data_receiver2_.get(); }

  LossyPacketTransport* packet_transport1() { return packet_transport1_.get(); }

  LossyPacketTransport* packet_transport2() { return packet_transport2_.get(); }

  cricket::SctpTransport* sctp_transport1() { return sctp_transport1_.get(); }

  cricket::SctpTransport* sctp_transport2() { return sctp_transport2_.get(); }

 private:
  void ReportError(std::string error) {
    rtc::CritScope cs(&lock_);
    errors_list_.push_back(std::move(error));
  }

  std::unique_ptr<LossyPacketTransport> packet_transport1_;
  std::unique_ptr<LossyPacketTransport> packet_transport2_;
  std::unique_ptr<SctpDataReceiver> data_receiver1_;
  std::unique_ptr<SctpDataReceiver> data_receiver2_;
  std::unique_ptr<cricket::SctpTransport> sctp_transport1_;
  std::unique_ptr<cricket::SctpTransport> sctp_transport2_;
  std::unique_ptr<SctpDataSender> sender1_;
  std::unique_ptr<SctpDataSender> sender2_;
  rtc::CriticalSection lock_;
  std::vector<std::string> errors_list_ RTC_GUARDED_BY(lock_);

  const uint32_t id_;
  const uint16_t port1_;
  const uint16_t port2_;
  rtc::Thread* const transport_thread1_;
  rtc::Thread* const transport_thread2_;
  const uint32_t messages_count_;
  const uint16_t packet_loss_percents_;
  const cricket::SendDataParams send_params_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpPingPong);
};

}  // namespace

namespace cricket {

// SCTP reliability testing.
class DISABLED_UsrSctpReliabilityTest : public ::testing::Test {};

/**
 * Kind of smoke test to verify test infrastructure works
 */
TEST_F(DISABLED_UsrSctpReliabilityTest,
       AllMessagesAreDeliveredOverReliableConnection) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint16_t packet_loss_percents = 0;
  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;

  constexpr uint32_t messages_count = 100;
  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    send_params);
  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(
      std::max<uint32_t>(messages_count * 100, kDefaultTimeout));
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

TEST_F(DISABLED_UsrSctpReliabilityTest,
       AllMessagesAreDeliveredOverLossyConnectionInOrder) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint16_t packet_loss_percents = 10;
  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;

  constexpr uint32_t messages_count = 10000;
  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    send_params);

  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(
      std::max<uint32_t>(messages_count * 100, kDefaultTimeout));
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

TEST_F(DISABLED_UsrSctpReliabilityTest,
       AllMessagesAreDeliveredOverLossyConnectionWithRetries) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint16_t packet_loss_percents = 10;
  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = false;
  send_params.reliable = false;
  send_params.max_rtx_count = INT_MAX;
  send_params.max_rtx_ms = INT_MAX;

  constexpr uint32_t messages_count = 10000;
  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    send_params);

  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(
      std::max<uint32_t>(messages_count * 100, kDefaultTimeout));
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

/**
 * Test reliability of usrsctp when underlying transport is lossy.
 * There was deadlock issues happen inside usrsctp when it is used
 * on bad networks: https://github.com/sctplab/usrsctp/issues/325
 * when many SCTP sockets alive simultaneously.
 */
TEST_F(DISABLED_UsrSctpReliabilityTest,
       AllMessagesAreDeliveredOverLossyConnectionConcurrentTests) {
  ThreadPool pool(16);

  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;
  constexpr uint32_t base_sctp_port = 5000;
  constexpr uint32_t messages_count = 200;
  constexpr uint16_t packet_loss_percents = 5;

  constexpr uint32_t parallel_ping_pongs = 16 * 1024;
  constexpr uint32_t total_ping_pong_tests = 16 * parallel_ping_pongs;
  constexpr uint32_t timeout = std::max<uint32_t>(
      messages_count * total_ping_pong_tests * 100 *
          std::max(1, packet_loss_percents * packet_loss_percents),
      kDefaultTimeout);

  std::queue<std::unique_ptr<SctpPingPong>> tests;

  for (uint32_t i = 0; i < total_ping_pong_tests; i++) {
    uint32_t port1 =
        base_sctp_port + (2 * i) % (UINT16_MAX - base_sctp_port - 1);

    auto test = std::make_unique<SctpPingPong>(
        i, port1, port1 + 1, pool.GetRandomThread(), pool.GetRandomThread(),
        messages_count, packet_loss_percents, send_params);

    EXPECT_TRUE(test->Start()) << rtc::join(test->GetErrorsList(), ';');
    tests.emplace(std::move(test));

    while (tests.size() >= parallel_ping_pongs) {
      auto& oldest_test = tests.front();
      oldest_test->WaitForCompletion(timeout);

      auto errors_list = oldest_test->GetErrorsList();
      EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
      tests.pop();
    }
  }

  while (!tests.empty()) {
    auto& oldest_test = tests.front();
    oldest_test->WaitForCompletion(timeout);

    auto errors_list = oldest_test->GetErrorsList();
    EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
    tests.pop();
  }
}

}  // namespace cricket
