/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pcc/test/pcc_printer.h"
#include "absl/memory/memory.h"

namespace webrtc {

PccStatePrinter::PccStatePrinter() = default;
PccStatePrinter::~PccStatePrinter() = default;

void PccStatePrinter::Attach(pcc::PccNetworkController* controller) {
  controller_ = controller;
}

bool PccStatePrinter::Attached() const {
  return controller_ != nullptr;
}

void PccStatePrinter::PrintHeaders(FILE* out) {
  fprintf(out,
          "pcc_mode pcc_sending_rate pcc_rtt_estimate utility_function "
          "pcc_state delay_gradient loss_rate receiver_rate");
}

void PccStatePrinter::PrintValues(FILE* out) {
  RTC_CHECK(controller_);
  pcc::PccNetworkController::DebugState debug(*controller_);
  fprintf(out, "%i %f %f %f %i %f %f %f", debug.mode,
          debug.actual_rate.bps() / 8.0, debug.rtt_estimate.seconds<double>(),
          debug.utility_function, debug.state, debug.delay_gradient,
          debug.loss_rate, debug.receiver_rate.bps() / 8.0);
}

NetworkControlUpdate PccStatePrinter::GetState(Timestamp at_time) const {
  RTC_CHECK(controller_);
  return controller_->CreateRateUpdate(at_time);
}

PccDebugFactory::PccDebugFactory(PccStatePrinter* printer,
                                 double rtt_gradient_coefficient,
                                 double loss_coefficient,
                                 double throughput_coefficient,
                                 double throughput_power,
                                 double rtt_gradient_threshold)
    : printer_(printer),
      rtt_gradient_coefficient_(rtt_gradient_coefficient),
      loss_coefficient_(loss_coefficient),
      throughput_coefficient_(throughput_coefficient),
      throughput_power_(throughput_power),
      rtt_gradient_threshold_(rtt_gradient_threshold) {}

std::unique_ptr<NetworkControllerInterface> PccDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = absl::make_unique<pcc::PccNetworkController>(
      config, rtt_gradient_coefficient_, loss_coefficient_,
      throughput_coefficient_, throughput_power_, rtt_gradient_threshold_);
  controller_ = static_cast<pcc::PccNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

pcc::PccNetworkController* PccDebugFactory::PccController() {
  return controller_;
}

}  // namespace webrtc
