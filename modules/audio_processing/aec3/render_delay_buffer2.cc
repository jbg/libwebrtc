/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_buffer.h"

#include <string.h>
#include <algorithm>
#include <numeric>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

size_t GetDownSamplingFactor(const EchoCanceller3Config& config) {
  // Do not use down sampling factor 8 if kill switch is triggered.
  return (config.delay.down_sampling_factor == 8 &&
          field_trial::IsEnabled("WebRTC-Aec3DownSamplingFactor8KillSwitch"))
             ? 4
             : config.delay.down_sampling_factor;
}

class RenderDelayBufferImpl2 final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl2(const EchoCanceller3Config& config, size_t num_bands);
  RenderDelayBufferImpl2() = delete;
  ~RenderDelayBufferImpl2() override;

  void Reset() override;
  BufferingEvent Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent PrepareCaptureProcessing() override;
  bool SetDelay(size_t delay) override;
  size_t Delay() const override { return MapInternalDelayToExternalDelay(); }
  size_t MaxDelay() const override {
    return blocks_.buffer.size() - 1 - buffer_headroom_;
  }
  RenderBuffer* GetRenderBuffer() override { return &echo_remover_buffer_; }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return low_rate_;
  }

  bool CausalDelay(size_t delay) const override;

  void SetAudioBufferDelay(size_t delay_ms) override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  size_t down_sampling_factor_;
  const int sub_block_size_;
  MatrixBuffer blocks_;
  VectorBuffer spectra_;
  FftBuffer ffts_;
  absl::optional<size_t> delay_;
  absl::optional<int> internal_delay_;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer low_rate_;
  Decimator render_decimator_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  std::vector<float> render_ds_;
  const int buffer_headroom_;
  bool last_call_was_render_ = false;
  int num_api_calls_in_a_row_ = 0;
  int max_observed_jitter_ = 1;
  size_t capture_call_counter_ = 0;
  size_t render_call_counter_ = 0;
  bool render_activity_ = false;
  size_t render_activity_counter_ = 0;
  absl::optional<size_t> external_audio_buffer_delay_;
  bool external_delay_verified_after_reset_ = false;
  size_t min_latency_blocks_ = 0;
  size_t excess_render_detection_counter_ = 0;
  size_t num_bands_;

  int MapExternalDelayToInternalDelay(size_t external_delay_blocks) const;
  int MapInternalDelayToExternalDelay() const;
  void ApplyDelay(int delay);
  void InsertBlock(const std::vector<std::vector<float>>& block,
                   int previous_write);
  bool DetectActiveRender(rtc::ArrayView<const float> x) const;
  bool DetectExcessRenderBlocks();
};

// Increases the write indices for the render buffers.
void IncreaseWriteIndices(int sub_block_size,
                          MatrixBuffer* blocks,
                          VectorBuffer* spectra,
                          FftBuffer* ffts,
                          DownsampledRenderBuffer* low_rate) {
  low_rate->UpdateWriteIndex(-sub_block_size);
  blocks->IncWriteIndex();
  spectra->DecWriteIndex();
  ffts->DecWriteIndex();
}

// Increases the read indices of the low rate render buffers.
void IncreaseLowRateReadIndices(int increment_size,
                                DownsampledRenderBuffer* low_rate) {
  RTC_DCHECK_NE(low_rate->read, low_rate->write);
  low_rate->UpdateReadIndex(-increment_size);
}

// Increases the read indices for the render buffers.
void IncreaseReadIndices(MatrixBuffer* blocks,
                         VectorBuffer* spectra,
                         FftBuffer* ffts) {
  if (blocks->read != blocks->write) {
    blocks->IncReadIndex();
    spectra->DecReadIndex();
    ffts->DecReadIndex();
  }
}

// Checks for a render buffer overrun.
bool RenderOverrun(const MatrixBuffer& b, const DownsampledRenderBuffer& l) {
  return l.read == l.write || b.read == b.write;
}

// Checks for a render buffer underrun. If the delay is not specified, only the
// low rate buffer underrun is counted as the delay offset for the other buffers
// is unknown.
bool RenderUnderrun(const absl::optional<int>& delay,
                    const MatrixBuffer& b,
                    const DownsampledRenderBuffer& l) {
  return l.read == l.write || (delay && b.read == b.write);
}

// Computes the latency in the buffer (the number of unread elements).
int BufferLatency(const DownsampledRenderBuffer& l) {
  return (l.buffer.size() + l.read - l.write) % l.buffer.size();
}

int RenderDelayBufferImpl2::instance_count_ = 0;

RenderDelayBufferImpl2::RenderDelayBufferImpl2(
    const EchoCanceller3Config& config,
    size_t num_bands)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      config_(config),
      down_sampling_factor_(GetDownSamplingFactor(config)),
      sub_block_size_(static_cast<int>(down_sampling_factor_ > 0
                                           ? kBlockSize / down_sampling_factor_
                                           : kBlockSize)),
      blocks_(GetRenderDelayBufferSize(down_sampling_factor_,
                                       config.delay.num_filters,
                                       config.filter.main.length_blocks),
              num_bands,
              kBlockSize),
      spectra_(blocks_.buffer.size(), kFftLengthBy2Plus1),
      ffts_(blocks_.buffer.size()),
      delay_(config_.delay.default_delay),
      echo_remover_buffer_(&blocks_, &spectra_, &ffts_),
      low_rate_(GetDownSampledBufferSize(down_sampling_factor_,
                                         config.delay.num_filters)),
      render_decimator_(down_sampling_factor_),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_(),
      render_ds_(sub_block_size_, 0.f),
      buffer_headroom_(config.filter.main.length_blocks),
      num_bands_(num_bands) {
  RTC_DCHECK_EQ(blocks_.buffer.size(), ffts_.buffer.size());
  RTC_DCHECK_EQ(spectra_.buffer.size(), ffts_.buffer.size());

  Reset();
}

RenderDelayBufferImpl2::~RenderDelayBufferImpl2() = default;

// Resets the buffer delays and clears the reported delays.
void RenderDelayBufferImpl2::Reset() {
  last_call_was_render_ = false;
  num_api_calls_in_a_row_ = 1;
  min_latency_blocks_ = 0;
  excess_render_detection_counter_ = 0;

  // Initialize the read index to one sub-block before the write index.
  low_rate_.read = low_rate_.OffsetIndex(low_rate_.write, sub_block_size_);

  // Check for any external audio buffer delay and whether it is feasible.
  if (external_audio_buffer_delay_) {
    const size_t headroom = 2;
    size_t external_delay_to_set;
    // Minimum delay is 1 (like the low-rate render buffer).
    if (*external_audio_buffer_delay_ <= headroom) {
      external_delay_to_set = 1;
    } else {
      external_delay_to_set = *external_audio_buffer_delay_ - headroom;
    }

    external_delay_to_set = std::min(external_delay_to_set, MaxDelay());

    // When an external delay estimate is available, use that delay as the
    // initial render buffer delay.
    internal_delay_ = external_delay_to_set;
    ApplyDelay(*internal_delay_);
    delay_ = MapInternalDelayToExternalDelay();

    external_delay_verified_after_reset_ = false;
  } else {
    // If an external delay estimate is not available, use that delay as the
    // initial delay. Set the render buffer delays to the default delay.
    ApplyDelay(config_.delay.default_delay);

    // Unset the delays which are set by SetDelay.
    delay_ = absl::nullopt;
    internal_delay_ = absl::nullopt;
  }
}

// Inserts a new block into the render buffers.
RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl2::Insert(
    const std::vector<std::vector<float>>& block) {
  ++render_call_counter_;
  if (delay_) {
    if (!last_call_was_render_) {
      last_call_was_render_ = true;
      num_api_calls_in_a_row_ = 1;
    } else {
      if (++num_api_calls_in_a_row_ > max_observed_jitter_) {
        max_observed_jitter_ = num_api_calls_in_a_row_;
        RTC_LOG(LS_WARNING)
            << "New max number api jitter observed at render block "
            << render_call_counter_ << ":  " << num_api_calls_in_a_row_
            << " blocks";
      }
    }
  }

  // Increase the write indices to where the new blocks should be written.
  const int previous_write = blocks_.write;
  IncreaseWriteIndices(sub_block_size_, &blocks_, &spectra_, &ffts_,
                       &low_rate_);

  // Allow overrun and do a reset when render overrun occurrs due to more render
  // data being inserted than capture data is received.
  BufferingEvent event = RenderOverrun(blocks_, low_rate_)
                             ? BufferingEvent::kRenderOverrun
                             : BufferingEvent::kNone;

  // Detect and update render activity.
  if (!render_activity_) {
    render_activity_counter_ += DetectActiveRender(block[0]) ? 1 : 0;
    render_activity_ = render_activity_counter_ >= 20;
  }

  // Insert the new render block into the specified position.
  InsertBlock(block, previous_write);

  if (event != BufferingEvent::kNone) {
    Reset();
  }

  return event;
}

// Prepares the render buffers for processing another capture block.
RenderDelayBuffer::BufferingEvent
RenderDelayBufferImpl2::PrepareCaptureProcessing() {
  RenderDelayBuffer::BufferingEvent event = BufferingEvent::kNone;
  ++capture_call_counter_;

  if (delay_) {
    if (last_call_was_render_) {
      last_call_was_render_ = false;
      num_api_calls_in_a_row_ = 1;
    } else {
      if (++num_api_calls_in_a_row_ > max_observed_jitter_) {
        max_observed_jitter_ = num_api_calls_in_a_row_;
        RTC_LOG(LS_WARNING)
            << "New max number api jitter observed at capture block "
            << capture_call_counter_ << ":  " << num_api_calls_in_a_row_
            << " blocks";
      }
    }
  }

  if (DetectExcessRenderBlocks()) {
    // Too many render blocks compared to capture blocks. Risk of delay ending
    // up before the filter used by the delay estimator.
    RTC_LOG(LS_WARNING) << "Excess render blocks detected at block "
                        << capture_call_counter_;
    Reset();
    event = BufferingEvent::kRenderOverrun;
  } else if (RenderUnderrun(internal_delay_, blocks_, low_rate_)) {
    // Don't increase the read indices of the low rate buffer if there is a
    // render underrun.
    RTC_LOG(LS_WARNING) << "Render buffer underrun detected at block "
                        << capture_call_counter_;
    IncreaseReadIndices(&blocks_, &spectra_, &ffts_);
    // Increasing the buffer index without increasing the low rate buffer index
    // means that the delay is reduced by one.
    if (delay_ && *delay_ > 0)
      delay_ = *delay_ - 1;
    event = BufferingEvent::kRenderUnderrun;
  } else {
    // Increase the read indices in the render buffers to point to the most
    // recent block to use in the capture processing.
    IncreaseLowRateReadIndices(sub_block_size_, &low_rate_);
    IncreaseReadIndices(&blocks_, &spectra_, &ffts_);
  }

  echo_remover_buffer_.SetRenderActivity(render_activity_);
  if (render_activity_) {
    render_activity_counter_ = 0;
    render_activity_ = false;
  }

  return event;
}

// Sets the delay and returns a bool indicating whether the delay was changed.
bool RenderDelayBufferImpl2::SetDelay(size_t delay) {
  if (!external_delay_verified_after_reset_ && external_audio_buffer_delay_ &&
      delay_) {
    int difference = static_cast<int>(delay) - static_cast<int>(*delay_);
    RTC_LOG(LS_WARNING) << "Mismatch between first estimated delay after reset "
                           "and external delay: "
                        << difference << " blocks";
    external_delay_verified_after_reset_ = true;
  }
  if (delay_ && *delay_ == delay) {
    return false;
  }
  delay_ = delay;

  // Compute the internal delay and limit the delay to the allowed range.
  int internal_delay = MapExternalDelayToInternalDelay(*delay_);
  internal_delay_ =
      std::min(MaxDelay(), static_cast<size_t>(std::max(internal_delay, 0)));

  // Apply the delay to the buffers.
  ApplyDelay(*internal_delay_);
  return true;
}

// Returns whether the specified delay is causal.
bool RenderDelayBufferImpl2::CausalDelay(size_t delay) const {
  // TODO(gustaf): Remove this from RenderDelayBuffer.
  return true;
}

void RenderDelayBufferImpl2::SetAudioBufferDelay(size_t delay_ms) {
  if (!external_audio_buffer_delay_) {
    RTC_LOG(LS_WARNING)
        << "Receiving a first reported externally buffer delay of " << delay_ms
        << " ms.";
  }

  // Convert delay from milliseconds to blocks (rounded down).
  external_audio_buffer_delay_ = delay_ms >> ((num_bands_ == 1) ? 1 : 2);
}

// Maps the externally computed delay to the delay used internally.
int RenderDelayBufferImpl2::MapExternalDelayToInternalDelay(
    size_t external_delay_blocks) const {
  const int latency = BufferLatency(low_rate_);
  RTC_DCHECK_LT(0, sub_block_size_);
  RTC_DCHECK_EQ(0, latency % sub_block_size_);
  int latency_blocks = latency / sub_block_size_;
  return latency_blocks + static_cast<int>(external_delay_blocks);
}

// Maps the internally used delay to the delay used externally.
int RenderDelayBufferImpl2::MapInternalDelayToExternalDelay() const {
  const int latency = BufferLatency(low_rate_);
  int latency_blocks = latency / sub_block_size_;
  int internal_delay = spectra_.read >= spectra_.write
                           ? spectra_.read - spectra_.write
                           : spectra_.size + spectra_.read - spectra_.write;

  return internal_delay - latency_blocks;
}

// Set the read indices according to the delay.
void RenderDelayBufferImpl2::ApplyDelay(int delay) {
  RTC_LOG(LS_WARNING) << "Applying internal delay of " << delay << " blocks.";
  blocks_.read = blocks_.OffsetIndex(blocks_.write, -delay);
  spectra_.read = spectra_.OffsetIndex(spectra_.write, delay);
  ffts_.read = ffts_.OffsetIndex(ffts_.write, delay);
}

// Inserts a block into the render buffers.
void RenderDelayBufferImpl2::InsertBlock(
    const std::vector<std::vector<float>>& block,
    int previous_write) {
  auto& b = blocks_;
  auto& lr = low_rate_;
  auto& ds = render_ds_;
  auto& f = ffts_;
  auto& s = spectra_;
  RTC_DCHECK_EQ(block.size(), b.buffer[b.write].size());
  for (size_t k = 0; k < block.size(); ++k) {
    RTC_DCHECK_EQ(block[k].size(), b.buffer[b.write][k].size());
    std::copy(block[k].begin(), block[k].end(), b.buffer[b.write][k].begin());
  }

  data_dumper_->DumpWav("aec3_render_decimator_input", block[0].size(),
                        block[0].data(), 16000, 1);
  render_decimator_.Decimate(block[0], ds);
  data_dumper_->DumpWav("aec3_render_decimator_output", ds.size(), ds.data(),
                        16000 / down_sampling_factor_, 1);
  std::copy(ds.rbegin(), ds.rend(), lr.buffer.begin() + lr.write);
  fft_.PaddedFft(block[0], b.buffer[previous_write][0], &f.buffer[f.write]);
  f.buffer[f.write].Spectrum(optimization_, s.buffer[s.write]);
}

bool RenderDelayBufferImpl2::DetectActiveRender(
    rtc::ArrayView<const float> x) const {
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  return x_energy > (config_.render_levels.active_render_limit *
                     config_.render_levels.active_render_limit) *
                        kFftLengthBy2;
}

bool RenderDelayBufferImpl2::DetectExcessRenderBlocks() {
  bool excess_render_detected = false;
  const size_t latency_blocks =
      static_cast<size_t>(BufferLatency(low_rate_) / sub_block_size_);
  // The recently seen minimum latency in blocks. Should be close to 0.
  min_latency_blocks_ = std::min(min_latency_blocks_, latency_blocks);
  // After processing a configurable number of blocks the minimum latency is
  // checked.
  if (++excess_render_detection_counter_ >=
      config_.buffering.excess_render_detection_interval_blocks) {
    // If the minimum latency is not lower than the threshold there have been
    // more render than capture frames.
    excess_render_detected = min_latency_blocks_ >
                             config_.buffering.max_allowed_excess_render_blocks;
    // Reset the counter and let the minimum latency be the current latency.
    min_latency_blocks_ = latency_blocks;
    excess_render_detection_counter_ = 0;
  }

  data_dumper_->DumpRaw("aec3_latency_blocks", latency_blocks);
  data_dumper_->DumpRaw("aec3_min_latency_blocks", min_latency_blocks_);
  data_dumper_->DumpRaw("aec3_excess_render_detected", excess_render_detected);
  return excess_render_detected;
}
}  // namespace

RenderDelayBuffer* RenderDelayBuffer::Create2(
    const EchoCanceller3Config& config,
    size_t num_bands) {
  return new RenderDelayBufferImpl2(config, num_bands);
}

}  // namespace webrtc
