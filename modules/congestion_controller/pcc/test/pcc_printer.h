/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_PCC_TEST_PCC_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_TEST_PCC_PRINTER_H_

#include <memory>

#include "modules/congestion_controller/pcc/pcc_factory.h"
#include "modules/congestion_controller/pcc/pcc_network_controller.h"
#include "modules/congestion_controller/test/controller_printer.h"

namespace webrtc {
class PccStatePrinter : public DebugStatePrinter {
 public:
  PccStatePrinter();
  ~PccStatePrinter() override;
  void Attach(pcc::PccNetworkController*);
  bool Attached() const override;

  void PrintHeaders(FILE* out) override;
  void PrintValues(FILE* out) override;

  NetworkControlUpdate GetState(Timestamp at_time) const override;

 private:
  pcc::PccNetworkController* controller_ = nullptr;
};

class PccDebugFactory : public PccNetworkControllerFactory {
 public:
  explicit PccDebugFactory(PccStatePrinter* printer,
                           double rtt_gradient_coefficient,
                           double loss_coefficient,
                           double throughput_coefficient,
                           double throughput_power,
                           double rtt_gradient_threshold);

  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  pcc::PccNetworkController* PccController();

 private:
  PccStatePrinter* printer_;
  pcc::PccNetworkController* controller_ = nullptr;
  // PccNetworkController parameters
  double rtt_gradient_coefficient_;
  double loss_coefficient_;
  double throughput_coefficient_;
  double throughput_power_;
  double rtt_gradient_threshold_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_PCC_TEST_PCC_PRINTER_H_
