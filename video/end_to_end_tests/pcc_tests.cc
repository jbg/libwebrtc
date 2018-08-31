/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <iostream>

#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "modules/congestion_controller/bbr/test/bbr_printer.h"
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"
#include "modules/congestion_controller/pcc/test/pcc_printer.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "video/end_to_end_tests/congestion_controller_test.h"

namespace webrtc {
namespace {
class GroundTruthPrinter {
 public:
  explicit GroundTruthPrinter(std::string filename) {
    output_file_ = fopen(filename.c_str(), "w");
    RTC_CHECK(output_file_);
    output_ = output_file_;
  }
  GroundTruthPrinter() { output_ = stdout; }
  ~GroundTruthPrinter() {
    if (output_file_)
      fclose(output_file_);
  }
  void PrintHeaders() {
    fprintf(output_, "time propagation_delay capacity cross_traffic\n");
  }
  void PrintStats(int64_t time_ms,
                  int64_t propagation_delay_ms,
                  int64_t capacity_kbps,
                  int64_t cross_traffic_bps) {
    fprintf(output_, "%.3lf %.3lf %.0lf %.0lf\n", time_ms / 1000.0,
            propagation_delay_ms / 1000.0, capacity_kbps * 1000 / 8.0,
            cross_traffic_bps / 8.0);
  }
  FILE* output_file_ = nullptr;
  FILE* output_ = nullptr;
};

constexpr int64_t kRunTimeMs = 60000;

using ::testing::Values;
using ::testing::Combine;
using ::testing::tuple;
using ::testing::make_tuple;
enum CcImpl : int { kNone = 0, kGcc = 1, kBbr = 2, kPcc = 3 };
enum AudioMode : int { kAudioOff = 0, kAudioOn = 1, kAudioBwe = 2 };
enum BbrTuning : int {
  kBbrTuningOff = 0,
  kBbrTargetRate = 1,
  kBbrInitialWindow = 2,
  kBbrBoth = 3
};

typedef pcc::PccNetworkController::MonitorIntervalLengthStrategy MIStrategy;

struct CallTestConfig {
  CcImpl send = kGcc;
  CcImpl ret = kNone;
  AudioMode audio_mode = kAudioOff;
  //  int capacity_kbps = 150;
  std::vector<int> capacity_array_kbps = {150};
  std::vector<int> times_of_capacity_change_s = {0};
  int delay_ms = 100;
  int cross_traffic_seed = 0;
  int delay_noise_ms = 0;
  int loss_percent = 0;
  // PCC parameters
  int64_t min_packets_number_per_interval;
  MIStrategy mi_length_strategy = MIStrategy::kFixed;
  double rtt_gradient_coefficient;
  double loss_coefficient;
  double throughput_coefficient;
  double throughput_power;
  double rtt_gradient_threshold;

  std::string AdditionalTrials() const {
    if (audio_mode == kAudioBwe) {
      return "/WebRTC-Audio-SendSideBwe/Enabled"
             "/WebRTC-SendSideBwe-WithOverhead/Enabled";
    }
    return "";
  }
  std::string Name() const {
    char pcc_name_buf[128];
    rtc::SimpleStringBuilder pcc_name(pcc_name_buf);
    pcc_name << "pcc";

    char raw_name[256];
    rtc::SimpleStringBuilder name(raw_name);
    RTC_DCHECK_GT(capacity_array_kbps.size(), 0);
    RTC_DCHECK_EQ(times_of_capacity_change_s.size(),
                  capacity_array_kbps.size());
    for (int capacity_kbps : capacity_array_kbps) {
      name.AppendFormat("_%i", capacity_kbps);
    }
    name.AppendFormat("kbps");

    for (int time : times_of_capacity_change_s) {
      name.AppendFormat("_%i", time);
    }
    name.AppendFormat("s");
    name.AppendFormat("_%ims_", delay_ms);
    if (delay_noise_ms > 0)
      name.AppendFormat("dn%i_", delay_noise_ms);
    if (loss_percent > 0)
      name.AppendFormat("lr%i_", loss_percent);
    if (send == kPcc) {
      name.AppendFormat("mp%ld_", min_packets_number_per_interval);
      name.AppendFormat("mils%i_", mi_length_strategy);
      name.AppendFormat("rttc%lf_", rtt_gradient_coefficient);
      name.AppendFormat("lc%lf_", loss_coefficient);
      name.AppendFormat("tc%lf_", throughput_coefficient);
      name.AppendFormat("tp%lf_", throughput_power);
      name.AppendFormat("rttt%lf_", rtt_gradient_threshold);
      name << pcc_name.str();
    } else if (send == kGcc) {
      name << "googcc";
    } else {
      name << "bbr";
    }

    if (ret == kGcc) {
      name << "_googcc";
    } else if (ret == kBbr) {
      name << "_bbr";
    } else if (ret == kPcc) {
      name << "_pcc";
    } else {
      name << "_none";
    }
    std::cout << "Full name = " << name.str() << std::endl;
    return name.str();
  }
};

class PccTestObserver : public test::BaseCongestionControllerTest {
 public:
  explicit PccTestObserver(CallTestConfig conf)
      : BaseCongestionControllerTest(
            kRunTimeMs,
            "/usr/local/google/home/koloskova/datadump/endtoend_test_gen/pcc_" +
                conf.Name()),
        cross_random_(std::max(conf.cross_traffic_seed, 1)),
        capacity_array_kbps_{conf.capacity_array_kbps},
        times_of_capacity_change_s_{conf.times_of_capacity_change_s},
        conf_(conf),
        send_truth_printer_(filepath_base_ + "_send.truth.txt"),
        recv_truth_printer_(filepath_base_ + "_recv.truth.txt") {
    config_.link_capacity_kbps = conf_.capacity_array_kbps[0];
    config_.queue_delay_ms = conf_.delay_ms;
    config_.delay_standard_deviation_ms = conf_.delay_noise_ms;
    config_.allow_reordering = false;
    config_.loss_percent = conf_.loss_percent;
    config_.queue_length_packets = 32;
    send_truth_printer_.PrintHeaders();
    recv_truth_printer_.PrintHeaders();
  }

  ~PccTestObserver() override {}

 private:
  size_t GetNumVideoStreams() const override { return 1; }
  size_t GetNumAudioStreams() const override {
    return (conf_.audio_mode != kAudioOff) ? 1 : 0;
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    BaseCongestionControllerTest::OnCallsCreated(sender_call, receiver_call);
    BitrateSettings settings;
    settings.max_bitrate_bps = 20000000;  // 1800000;
    settings.start_bitrate_bps = 300000;
    settings.min_bitrate_bps = 30000;
    sender_call->GetTransportControllerSend()->SetClientBitratePreferences(
        settings);
    receiver_call->GetTransportControllerSend()->SetClientBitratePreferences(
        settings);
  }
  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    std::unique_ptr<SimulatedNetwork> network_simulation =
        absl::make_unique<webrtc::SimulatedNetwork>(config_, 1);
    network_simulation_ = network_simulation.get();
    auto send_pipe = absl::make_unique<webrtc::FakeNetworkPipe>(
        Clock::GetRealTimeClock(), absl::move(network_simulation));
    send_pipe_ = send_pipe.get();
    send_transport_ = new test::PacketTransport(
        task_queue, sender_call, this, test::PacketTransport::kSender,
        test::CallTest::payload_type_map_, std::move(send_pipe));
    return send_transport_;
  }

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    send_config->send_codec_spec->transport_cc_enabled = true;

    send_config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri, 8));
    for (AudioReceiveStream::Config& recv_config : *receive_configs) {
      recv_config.rtp.transport_cc = true;
      recv_config.rtp.extensions = send_config->rtp.extensions;
      recv_config.rtp.remote_ssrc = send_config->rtp.ssrc;
    }
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    encoder_config->max_bitrate_bps = 20000000;
  }

  void PerformTest() override {
    Clock* clock = Clock::GetRealTimeClock();
    int64_t first_update_ms = clock->TimeInMilliseconds();
    int64_t last_state_update_ms = 0;
    do {
      int64_t now_ms = clock->TimeInMilliseconds();
      if (now_ms - first_update_ms > kRunTimeMs)
        break;
      if ((idx_ < times_of_capacity_change_s_.size()) &&
          (0.001 * (now_ms - first_update_ms) >
           times_of_capacity_change_s_[idx_])) {
        config_.link_capacity_kbps = capacity_array_kbps_[idx_];
        config_.queue_length_packets = 0;
        network_simulation_->SetConfig(config_);
        std::cout << "changed capacity on: " << capacity_array_kbps_[idx_]
                  << std::endl;
        std::cout << "current time (ms): " << (now_ms - first_update_ms)
                  << std::endl;
        ++idx_;
      }
      if (now_ms - last_state_update_ms > 100) {
        last_state_update_ms = now_ms;
        PrintStates(now_ms);
        PrintStats(now_ms);
        send_truth_printer_.PrintStats(now_ms, config_.queue_delay_ms,
                                       config_.link_capacity_kbps, 0);
        recv_truth_printer_.PrintStats(now_ms, config_.queue_delay_ms,
                                       config_.link_capacity_kbps, 0);
      }
    } while (!observation_complete_.Wait(5));
  }

  std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
            std::unique_ptr<DebugStatePrinter>>
  CreateReturnCCFactory(RtcEventLog* event_log) override {
    if (conf_.ret == kPcc) {
      auto pcc_printer = absl::make_unique<PccStatePrinter>();
      auto return_factory = absl::make_unique<PccDebugFactory>(
          pcc_printer.get(), conf_.rtt_gradient_coefficient,
          conf_.loss_coefficient, conf_.throughput_coefficient,
          conf_.throughput_power, conf_.rtt_gradient_threshold);
      return {std::move(return_factory), std::move(pcc_printer)};
    } else {
      RTC_DCHECK_EQ(conf_.ret, kGcc);
      auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
      auto return_factory =
          absl::make_unique<GoogCcDebugFactory>(event_log, goog_printer.get());
      return {std::move(return_factory), std::move(goog_printer)};
    }
  }
  std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
            std::unique_ptr<DebugStatePrinter>>
  CreateSendCCFactory(RtcEventLog* event_log) override {
    if (conf_.send == kPcc) {
      auto pcc_printer = absl::make_unique<PccStatePrinter>();
      auto send_factory = absl::make_unique<PccDebugFactory>(
          pcc_printer.get(), conf_.rtt_gradient_coefficient,
          conf_.loss_coefficient, conf_.throughput_coefficient,
          conf_.throughput_power, conf_.rtt_gradient_threshold);
      return {std::move(send_factory), std::move(pcc_printer)};
    } else {
      RTC_DCHECK_EQ(conf_.send, kGcc);
      auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
      auto send_factory =
          absl::make_unique<GoogCcDebugFactory>(event_log, goog_printer.get());
      return {std::move(send_factory), std::move(goog_printer)};
    }
  }

  webrtc::Random cross_random_;
  FakeNetworkPipe::Config config_;
  std::vector<int> capacity_array_kbps_;
  std::vector<int> times_of_capacity_change_s_;
  size_t idx_ = 1;
  const CallTestConfig conf_;
  GroundTruthPrinter send_truth_printer_;
  GroundTruthPrinter recv_truth_printer_;
  test::PacketTransport* send_transport_ = nullptr;
  FakeNetworkPipe* send_pipe_ = nullptr;
  SimulatedNetwork* network_simulation_ = nullptr;
};
}  // namespace

std::string ConvertMonitorIntervalStrategyToString(
    MIStrategy monitor_interval_strategy) {
  if (monitor_interval_strategy == MIStrategy::kAdaptive) {
    return "kAdaptive";
  }
  return "kFixed";
}

class PccEndToEndTest
    : public test::CallTest,
      public testing::WithParamInterface<tuple<CcImpl,
                                               std::vector<int>,
                                               std::vector<int>,
                                               int,
                                               int,
                                               int,
                                               int64_t,
                                               MIStrategy,
                                               double,
                                               double,
                                               double,
                                               double,
                                               double>> {
 public:
  PccEndToEndTest() {
    conf_.send = static_cast<CcImpl>(::testing::get<0>(GetParam()));
    conf_.capacity_array_kbps = ::testing::get<1>(GetParam());
    conf_.times_of_capacity_change_s = ::testing::get<2>(GetParam());
    conf_.delay_ms = ::testing::get<3>(GetParam());
    conf_.loss_percent = ::testing::get<4>(GetParam());
    conf_.delay_noise_ms = ::testing::get<5>(GetParam());
    conf_.min_packets_number_per_interval = ::testing::get<6>(GetParam());
    conf_.mi_length_strategy = ::testing::get<7>(GetParam());
    conf_.rtt_gradient_coefficient = ::testing::get<8>(GetParam());
    conf_.loss_coefficient = ::testing::get<9>(GetParam());
    conf_.throughput_coefficient = ::testing::get<10>(GetParam());
    conf_.throughput_power = ::testing::get<11>(GetParam());
    conf_.rtt_gradient_threshold = ::testing::get<12>(GetParam());

    field_trial_.reset(new test::ScopedFieldTrials(
        "WebRTC-TaskQueueCongestionControl/Enabled"
        "/WebRTC-PacerPushbackExperiment/Enabled"
        "/WebRTC-Pacer-DrainQueue/Disabled"
        "/WebRTC-Pacer-PadInSilence/Enabled"
        "/WebRTC-Pacer-BlockAudio/Disabled"
        "/WebRTC-BwePccConfig/min_packets_number_per_interval:" +
        std::to_string(conf_.min_packets_number_per_interval) +
        ",monitor_interval_length_strategy:" +
        ConvertMonitorIntervalStrategyToString(conf_.mi_length_strategy) +
        conf_.AdditionalTrials() + "/"));
  }
  ~PccEndToEndTest() {}
  CallTestConfig conf_;

 private:
  std::unique_ptr<test::ScopedFieldTrials> field_trial_;
};

TEST_P(PccEndToEndTest, SendTraffic) {
  PccTestObserver test(conf_);
  RunBaseTest(&test);
}

// capacity_array_kbps, times_of_capacity_change_s,
// delay_ms, loss_percent, delay_noise_ms,
// min_packets_number_per_interval, mi_length_strategy,
// rtt_gradient_coefficient, loss_coefficient, throughput_coefficient,
// throughput_power, rtt_gradient_threshold
const double delay_gradient_coef = 0.005;
const double throughput = 0.004;
const double loss_coef = 10;
const int loss_rate_percent = 3;
const double delay_noise_ms = 40;

INSTANTIATE_TEST_CASE_P(PccDebug,
                        PccEndToEndTest,
                        Values(make_tuple(kPcc,
                                          std::vector<int>{100, 60, 100},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          delay_noise_ms,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,  // Rtt gradient
                                          loss_coef,            // loss
                                          throughput,           // throughput
                                          0.9,                  // power
                                          0.02),
                               make_tuple(kPcc,
                                          std::vector<int>{500, 300, 500},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          delay_noise_ms,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,
                                          loss_coef,
                                          throughput,
                                          0.9,
                                          0.02),
                               make_tuple(kPcc,
                                          std::vector<int>{5000, 3000, 5000},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          delay_noise_ms,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,
                                          loss_coef,
                                          throughput,
                                          0.9,
                                          0.02)));

INSTANTIATE_TEST_CASE_P(GoogCCForPcc,
                        PccEndToEndTest,
                        Values(make_tuple(kGcc,
                                          std::vector<int>{100, 60, 100},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          0,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,  // Rtt gradient
                                          0,                    // loss
                                          throughput,           // throughput
                                          1,                    // power
                                          0.02),
                               make_tuple(kGcc,
                                          std::vector<int>{500, 300, 500},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          0,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,
                                          0,
                                          throughput,
                                          1,
                                          0.02),
                               make_tuple(kGcc,
                                          std::vector<int>{5000, 3000, 5000},
                                          std::vector<int>{0, 20, 40},
                                          200,
                                          loss_rate_percent,
                                          0,
                                          10,
                                          MIStrategy::kFixed,
                                          delay_gradient_coef,
                                          0,
                                          throughput,
                                          1,
                                          0.02)));

}  // namespace webrtc
