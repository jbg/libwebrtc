/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/aec3/vector_buffer.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr int kLowFrequencyLimit = kFftLengthBy2 / 2;
constexpr float kMaxErleLf = 8.f;
constexpr float kMaxErleHf = 1.5f;
constexpr float kMinErle = 1.0f;
constexpr float kTrueErle = 10.f;
constexpr float kTrueErleOnsets = 1.0f;
constexpr float kEchoPathGain = 3.f;

void VerifyErleBands(rtc::ArrayView<const float> erle,
                     float reference_lf,
                     float reference_hf) {
  std::for_each(
      erle.begin(), erle.begin() + kLowFrequencyLimit,
      [reference_lf](float a) { EXPECT_NEAR(reference_lf, a, 0.001); });
  std::for_each(
      erle.begin() + kLowFrequencyLimit, erle.end(),
      [reference_hf](float a) { EXPECT_NEAR(reference_hf, a, 0.001); });
}

void VerifyErle(rtc::ArrayView<const float> erle,
                float erle_time_domain,
                float reference_lf,
                float reference_hf) {
  VerifyErleBands(erle, reference_lf, reference_hf);
  EXPECT_NEAR(reference_lf, erle_time_domain, 0.5);
}

void FormFarendTimeFrame(rtc::ArrayView<float> x) {
  Random random_generator(42U);
  for (auto& v : x) {
    v = random_generator.Rand<float>() * 22.f * 1000.f;
  }
}

void FormFarendFrame(const RenderBuffer& render_buffer,
                     std::array<float, kFftLengthBy2Plus1>* X2,
                     std::array<float, kFftLengthBy2Plus1>* E2,
                     std::array<float, kFftLengthBy2Plus1>* Y2,
                     float erle) {
  const auto& spectrum_buffer = render_buffer.GetSpectrumBuffer();
  const auto& X2_from_buffer = spectrum_buffer.buffer[spectrum_buffer.write];
  std::copy(X2_from_buffer.begin(), X2_from_buffer.end(), X2->begin());
  std::transform(X2->begin(), X2->end(), Y2->begin(),
                 [](float a) { return a * kEchoPathGain * kEchoPathGain; });
  std::transform(Y2->begin(), Y2->end(), E2->begin(),
                 [erle](float a) { return a / erle; });

}  // namespace

void FormNearendFrame(rtc::ArrayView<float> x,
                      std::array<float, kFftLengthBy2Plus1>* X2,
                      std::array<float, kFftLengthBy2Plus1>* E2,
                      std::array<float, kFftLengthBy2Plus1>* Y2) {
  x[0] = 0.f;
  X2->fill(0.f);
  Y2->fill(500.f * 1000.f * 1000.f);
  E2->fill((*Y2)[0]);
}

void GetFilterFreq(std::vector<std::array<float, kFftLengthBy2Plus1>>&
                       filter_frequency_response,
                   size_t delay_headroom_blocks) {
  RTC_DCHECK_GE(filter_frequency_response.size(), delay_headroom_blocks);
  for (auto& block_freq_resp : filter_frequency_response) {
    block_freq_resp.fill(0.f);
  }

  for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
    filter_frequency_response[delay_headroom_blocks][k] = kEchoPathGain;
  }
}

}  // namespace

TEST(ErleEstimator, VerifyErleIncreaseAndHold) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;
  EchoCanceller3Config config;
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::array<float, kFftLengthBy2Plus1>> filter_frequency_response(
      config.filter.main.length_blocks);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create2(config, 3));

  GetFilterFreq(filter_frequency_response, config.delay.delay_headroom_blocks);

  ErleEstimator estimator(0, kMinErle, kMaxErleLf, kMaxErleHf, 1,
                          config.filter.main.length_blocks,
                          config.delay.delay_headroom_blocks);

  FormFarendTimeFrame(x[0]);
  render_delay_buffer->Insert(x);
  // Verifies that the ERLE estimate is properly increased to higher values.
  FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), &X2, &E2, &Y2,
                  kTrueErle);
  for (size_t k = 0; k < 200; ++k) {
    render_delay_buffer->Insert(x);
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, true, true);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMaxErleLf, kMaxErleHf);

  FormNearendFrame(x[0], &X2, &E2, &Y2);
  // Verifies that the ERLE is not immediately decreased during nearend
  // activity.
  for (size_t k = 0; k < 50; ++k) {
    render_delay_buffer->Insert(x);
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, true, true);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMaxErleLf, kMaxErleHf);
}

TEST(ErleEstimator, VerifyErleTrackingOnOnsets) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;
  EchoCanceller3Config config;
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::array<float, kFftLengthBy2Plus1>> filter_frequency_response(
      config.filter.main.length_blocks);

  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create2(config, 3));

  GetFilterFreq(filter_frequency_response, config.delay.delay_headroom_blocks);

  ErleEstimator estimator(0, kMinErle, kMaxErleLf, kMaxErleHf, 1,
                          config.filter.main.length_blocks,
                          config.delay.delay_headroom_blocks);

  FormFarendTimeFrame(x[0]);
  render_delay_buffer->Insert(x);

  for (size_t burst = 0; burst < 20; ++burst) {
    FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), &X2, &E2, &Y2,
                    kTrueErleOnsets);
    for (size_t k = 0; k < 10; ++k) {
      render_delay_buffer->Insert(x);
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2, true, true);
    }
    FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), &X2, &E2, &Y2,
                    kTrueErle);
    for (size_t k = 0; k < 200; ++k) {
      render_delay_buffer->Insert(x);
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2, true, true);
    }
    FormNearendFrame(x[0], &X2, &E2, &Y2);
    for (size_t k = 0; k < 300; ++k) {
      render_delay_buffer->Insert(x);
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2, true, true);
    }
  }
  VerifyErleBands(estimator.ErleOnsets(), kMinErle, kMinErle);
  FormNearendFrame(x[0], &X2, &E2, &Y2);
  for (size_t k = 0; k < 1000; k++) {
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, true, true);
  }
  // Verifies that during ne activity, Erle converges to the Erle for onsets.
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMinErle, kMinErle);
}

}  // namespace webrtc
