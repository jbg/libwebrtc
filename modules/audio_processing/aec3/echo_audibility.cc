/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_audibility.h"

#include <algorithm>
#include <cmath>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"

namespace webrtc {

EchoAudibility::EchoAudibility() {
  Reset();
}

EchoAudibility::~EchoAudibility() = default;

void EchoAudibility::Update(const RenderBuffer& render_buffer,
                            size_t delay_blocks,
                            bool external_delay_seen) {
  UpdateRenderNoiseEstimator(render_buffer, external_delay_seen);

  if (external_delay_seen) {
    UpdateRenderStationarityFlags(render_buffer, delay_blocks);
  }
}

void EchoAudibility::Reset() {
  render_stationarity_.Reset();
  non_zero_render_seen_ = false;
  render_spectrum_write_prev_ = rtc::nullopt;
}

void EchoAudibility::UpdateRenderStationarityFlags(
    const RenderBuffer& render_buffer,
    size_t delay_blocks) {
  int idx_at_delay =
      render_buffer.OffsetSpectrumIndex(render_buffer.Position(), delay_blocks);
  size_t num_lookahead = render_buffer.Headroom() - delay_blocks + 1;
  render_stationarity_.UpdateStationarityFlags(
      render_buffer.GetSpectrumBuffer(), idx_at_delay, num_lookahead);
}

void EchoAudibility::UpdateRenderNoiseEstimator(
    const RenderBuffer& render_buffer,
    bool external_delay_seen) {
  if (!render_spectrum_write_prev_) {
    render_spectrum_write_prev_ = render_buffer.GetWritePositionSpectrum();
    render_block_write_prev_ = render_buffer.GetWritePositionBlocks();
    return;
  }
  int render_spectrum_write_current = render_buffer.GetWritePositionSpectrum();
  if (!non_zero_render_seen_ && !external_delay_seen) {
    non_zero_render_seen_ = !IsRenderTooLow(render_buffer);
  }
  if (non_zero_render_seen_) {
    for (int idx = render_spectrum_write_prev_.value();
         idx != render_spectrum_write_current;
         idx = render_buffer.DecIdx(idx)) {
      render_stationarity_.UpdateNoiseEstimator(
          render_buffer.SpectrumAtIndex(idx));
    }
  }
  render_spectrum_write_prev_ = render_spectrum_write_current;
}

bool EchoAudibility::IsRenderTooLow(const RenderBuffer& render_buffer) {
  bool too_low = false;
  int render_block_write_current = render_buffer.GetWritePositionBlocks();
  if (render_block_write_current == render_block_write_prev_) {
    too_low = true;
  }
  for (int idx = render_block_write_prev_; idx != render_block_write_current;
       idx = render_buffer.IncIdx(idx)) {
    auto block = render_buffer.BlockAtIndex(idx)[0];
    auto r = std::minmax_element(block.cbegin(), block.cend());
    float max_abs = std::max(std::fabs(*r.first), std::fabs(*r.second));
    if (max_abs < 10) {
      too_low = true;  // Discards all blocks if one of them is too low.
      break;
    }
  }
  render_block_write_prev_ = render_block_write_current;
  return too_low;
}

}  // namespace webrtc
