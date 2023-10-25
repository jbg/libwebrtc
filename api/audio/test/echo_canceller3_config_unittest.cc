/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/echo_canceller3_config.h"

#include "test/gtest.h"

namespace webrtc {

namespace {
bool operator==(const EchoCanceller3Config& c1,
                const EchoCanceller3Config& c2) {
  return c1.buffering.excess_render_detection_interval_blocks ==
             c2.buffering.excess_render_detection_interval_blocks &&
         c1.buffering.max_allowed_excess_render_blocks ==
             c2.buffering.max_allowed_excess_render_blocks &&
         c1.delay.default_delay == c2.delay.default_delay &&
         c1.delay.down_sampling_factor == c2.delay.down_sampling_factor &&
         c1.delay.num_filters == c2.delay.num_filters &&
         c1.delay.delay_headroom_samples == c2.delay.delay_headroom_samples &&
         c1.delay.hysteresis_limit_blocks == c2.delay.hysteresis_limit_blocks &&
         c1.delay.fixed_capture_delay_samples ==
             c2.delay.fixed_capture_delay_samples &&
         c1.delay.delay_estimate_smoothing ==
             c2.delay.delay_estimate_smoothing &&
         c1.delay.delay_estimate_smoothing_delay_found ==
             c2.delay.delay_estimate_smoothing_delay_found &&
         c1.delay.delay_candidate_detection_threshold ==
             c2.delay.delay_candidate_detection_threshold &&
         c1.delay.delay_selection_thresholds.initial ==
             c2.delay.delay_selection_thresholds.initial &&
         c1.delay.delay_selection_thresholds.converged ==
             c2.delay.delay_selection_thresholds.converged &&
         c1.delay.use_external_delay_estimator ==
             c2.delay.use_external_delay_estimator &&
         c1.delay.log_warning_on_delay_changes ==
             c2.delay.log_warning_on_delay_changes &&
         c1.delay.render_alignment_mixing.downmix ==
             c2.delay.render_alignment_mixing.downmix &&
         c1.delay.render_alignment_mixing.adaptive_selection ==
             c2.delay.render_alignment_mixing.adaptive_selection &&
         c1.delay.render_alignment_mixing.activity_power_threshold ==
             c2.delay.render_alignment_mixing.activity_power_threshold &&
         c1.delay.render_alignment_mixing.prefer_first_two_channels ==
             c2.delay.render_alignment_mixing.prefer_first_two_channels &&
         c1.delay.capture_alignment_mixing.downmix ==
             c2.delay.capture_alignment_mixing.downmix &&
         c1.delay.capture_alignment_mixing.adaptive_selection ==
             c2.delay.capture_alignment_mixing.adaptive_selection &&
         c1.delay.capture_alignment_mixing.activity_power_threshold ==
             c2.delay.capture_alignment_mixing.activity_power_threshold &&
         c1.delay.capture_alignment_mixing.prefer_first_two_channels ==
             c2.delay.capture_alignment_mixing.prefer_first_two_channels &&
         c1.delay.detect_pre_echo == c2.delay.detect_pre_echo &&
         c1.filter.refined.length_blocks == c2.filter.refined.length_blocks &&
         c1.filter.refined.leakage_converged ==
             c2.filter.refined.leakage_converged &&
         c1.filter.refined.leakage_diverged ==
             c2.filter.refined.leakage_diverged &&
         c1.filter.refined.error_floor == c2.filter.refined.error_floor &&
         c1.filter.refined.error_ceil == c2.filter.refined.error_ceil &&
         c1.filter.refined.noise_gate == c2.filter.refined.noise_gate &&
         c1.filter.refined_initial.length_blocks ==
             c2.filter.refined_initial.length_blocks &&
         c1.filter.refined_initial.leakage_converged ==
             c2.filter.refined_initial.leakage_converged &&
         c1.filter.refined_initial.leakage_diverged ==
             c2.filter.refined_initial.leakage_diverged &&
         c1.filter.refined_initial.error_floor ==
             c2.filter.refined_initial.error_floor &&
         c1.filter.refined_initial.error_ceil ==
             c2.filter.refined_initial.error_ceil &&
         c1.filter.refined_initial.noise_gate ==
             c2.filter.refined_initial.noise_gate &&
         c1.filter.coarse.length_blocks == c2.filter.coarse.length_blocks &&
         c1.filter.coarse.rate == c2.filter.coarse.rate &&
         c1.filter.coarse.noise_gate == c2.filter.coarse.noise_gate &&
         c1.filter.coarse_initial.length_blocks ==
             c2.filter.coarse_initial.length_blocks &&
         c1.filter.coarse_initial.rate == c2.filter.coarse_initial.rate &&
         c1.filter.coarse_initial.noise_gate ==
             c2.filter.coarse_initial.noise_gate &&
         c1.filter.config_change_duration_blocks ==
             c2.filter.config_change_duration_blocks &&
         c1.filter.initial_state_seconds == c2.filter.initial_state_seconds &&
         c1.filter.coarse_reset_hangover_blocks ==
             c2.filter.coarse_reset_hangover_blocks &&
         c1.filter.conservative_initial_phase ==
             c2.filter.conservative_initial_phase &&
         c1.filter.enable_coarse_filter_output_usage ==
             c2.filter.enable_coarse_filter_output_usage &&
         c1.filter.use_linear_filter == c2.filter.use_linear_filter &&
         c1.filter.high_pass_filter_echo_reference ==
             c2.filter.high_pass_filter_echo_reference &&
         c1.filter.export_linear_aec_output ==
             c2.filter.export_linear_aec_output &&
         c1.erle.min == c2.erle.min && c1.erle.max_l == c2.erle.max_l &&
         c1.erle.max_h == c2.erle.max_h &&
         c1.erle.onset_detection == c2.erle.onset_detection &&
         c1.erle.num_sections == c2.erle.num_sections &&
         c1.erle.clamp_quality_estimate_to_zero ==
             c2.erle.clamp_quality_estimate_to_zero &&
         c1.erle.clamp_quality_estimate_to_one ==
             c2.erle.clamp_quality_estimate_to_one &&
         c1.ep_strength.default_gain == c2.ep_strength.default_gain &&
         c1.ep_strength.default_len == c2.ep_strength.default_len &&
         c1.ep_strength.nearend_len == c2.ep_strength.nearend_len &&
         c1.ep_strength.echo_can_saturate == c2.ep_strength.echo_can_saturate &&
         c1.ep_strength.bounded_erl == c2.ep_strength.bounded_erl &&
         c1.ep_strength.erle_onset_compensation_in_dominant_nearend ==
             c2.ep_strength.erle_onset_compensation_in_dominant_nearend &&
         c1.ep_strength.use_conservative_tail_frequency_response ==
             c2.ep_strength.use_conservative_tail_frequency_response &&
         c1.echo_audibility.low_render_limit ==
             c2.echo_audibility.low_render_limit &&
         c1.echo_audibility.normal_render_limit ==
             c2.echo_audibility.normal_render_limit &&
         c1.echo_audibility.floor_power == c2.echo_audibility.floor_power &&
         c1.echo_audibility.audibility_threshold_lf ==
             c2.echo_audibility.audibility_threshold_lf &&
         c1.echo_audibility.audibility_threshold_mf ==
             c2.echo_audibility.audibility_threshold_mf &&
         c1.echo_audibility.audibility_threshold_hf ==
             c2.echo_audibility.audibility_threshold_hf &&
         c1.echo_audibility.use_stationarity_properties ==
             c2.echo_audibility.use_stationarity_properties &&
         c1.echo_audibility.use_stationarity_properties_at_init ==
             c2.echo_audibility.use_stationarity_properties_at_init &&
         c1.render_levels.active_render_limit ==
             c2.render_levels.active_render_limit &&
         c1.render_levels.poor_excitation_render_limit ==
             c2.render_levels.poor_excitation_render_limit &&
         c1.render_levels.poor_excitation_render_limit_ds8 ==
             c2.render_levels.poor_excitation_render_limit_ds8 &&
         c1.render_levels.render_power_gain_db ==
             c2.render_levels.render_power_gain_db &&
         c1.echo_removal_control.has_clock_drift ==
             c2.echo_removal_control.has_clock_drift &&
         c1.echo_removal_control.linear_and_stable_echo_path ==
             c2.echo_removal_control.linear_and_stable_echo_path &&
         c1.echo_model.noise_floor_hold == c2.echo_model.noise_floor_hold &&
         c1.echo_model.min_noise_floor_power ==
             c2.echo_model.min_noise_floor_power &&
         c1.echo_model.stationary_gate_slope ==
             c2.echo_model.stationary_gate_slope &&
         c1.echo_model.noise_gate_power == c2.echo_model.noise_gate_power &&
         c1.echo_model.noise_gate_slope == c2.echo_model.noise_gate_slope &&
         c1.echo_model.render_pre_window_size ==
             c2.echo_model.render_pre_window_size &&
         c1.echo_model.render_post_window_size ==
             c2.echo_model.render_post_window_size &&
         c1.echo_model.model_reverb_in_nonlinear_mode ==
             c2.echo_model.model_reverb_in_nonlinear_mode &&
         c1.comfort_noise.noise_floor_dbfs ==
             c2.comfort_noise.noise_floor_dbfs &&
         c1.suppressor.nearend_average_blocks ==
             c2.suppressor.nearend_average_blocks &&
         c1.suppressor.normal_tuning.mask_lf.enr_transparent ==
             c2.suppressor.normal_tuning.mask_lf.enr_transparent &&
         c1.suppressor.normal_tuning.mask_lf.enr_suppress ==
             c2.suppressor.normal_tuning.mask_lf.enr_suppress &&
         c1.suppressor.normal_tuning.mask_lf.emr_transparent ==
             c2.suppressor.normal_tuning.mask_lf.emr_transparent &&
         c1.suppressor.normal_tuning.mask_hf.enr_transparent ==
             c2.suppressor.normal_tuning.mask_hf.enr_transparent &&
         c1.suppressor.normal_tuning.mask_hf.enr_suppress ==
             c2.suppressor.normal_tuning.mask_hf.enr_suppress &&
         c1.suppressor.normal_tuning.mask_hf.emr_transparent ==
             c2.suppressor.normal_tuning.mask_hf.emr_transparent &&
         c1.suppressor.normal_tuning.max_inc_factor ==
             c2.suppressor.normal_tuning.max_inc_factor &&
         c1.suppressor.normal_tuning.max_dec_factor_lf ==
             c2.suppressor.normal_tuning.max_dec_factor_lf &&
         c1.suppressor.nearend_tuning.mask_lf.enr_transparent ==
             c2.suppressor.nearend_tuning.mask_lf.enr_transparent &&
         c1.suppressor.nearend_tuning.mask_lf.enr_suppress ==
             c2.suppressor.nearend_tuning.mask_lf.enr_suppress &&
         c1.suppressor.nearend_tuning.mask_lf.emr_transparent ==
             c2.suppressor.nearend_tuning.mask_lf.emr_transparent &&
         c1.suppressor.nearend_tuning.mask_hf.enr_transparent ==
             c2.suppressor.nearend_tuning.mask_hf.enr_transparent &&
         c1.suppressor.nearend_tuning.mask_hf.enr_suppress ==
             c2.suppressor.nearend_tuning.mask_hf.enr_suppress &&
         c1.suppressor.nearend_tuning.mask_hf.emr_transparent ==
             c2.suppressor.nearend_tuning.mask_hf.emr_transparent &&
         c1.suppressor.nearend_tuning.max_inc_factor ==
             c2.suppressor.nearend_tuning.max_inc_factor &&
         c1.suppressor.nearend_tuning.max_dec_factor_lf ==
             c2.suppressor.nearend_tuning.max_dec_factor_lf &&
         c1.suppressor.lf_smoothing_during_initial_phase ==
             c2.suppressor.lf_smoothing_during_initial_phase &&
         c1.suppressor.last_permanent_lf_smoothing_band ==
             c2.suppressor.last_permanent_lf_smoothing_band &&
         c1.suppressor.last_lf_smoothing_band ==
             c2.suppressor.last_lf_smoothing_band &&
         c1.suppressor.last_lf_band == c2.suppressor.last_lf_band &&
         c1.suppressor.first_hf_band == c2.suppressor.first_hf_band &&
         c1.suppressor.dominant_nearend_detection.enr_threshold ==
             c2.suppressor.dominant_nearend_detection.enr_threshold &&
         c1.suppressor.dominant_nearend_detection.enr_exit_threshold ==
             c2.suppressor.dominant_nearend_detection.enr_exit_threshold &&
         c1.suppressor.dominant_nearend_detection.snr_threshold ==
             c2.suppressor.dominant_nearend_detection.snr_threshold &&
         c1.suppressor.dominant_nearend_detection.hold_duration ==
             c2.suppressor.dominant_nearend_detection.hold_duration &&
         c1.suppressor.dominant_nearend_detection.trigger_threshold ==
             c2.suppressor.dominant_nearend_detection.trigger_threshold &&
         c1.suppressor.dominant_nearend_detection.use_during_initial_phase ==
             c2.suppressor.dominant_nearend_detection
                 .use_during_initial_phase &&
         c1.suppressor.dominant_nearend_detection.use_unbounded_echo_spectrum ==
             c2.suppressor.dominant_nearend_detection
                 .use_unbounded_echo_spectrum &&
         c1.suppressor.subband_nearend_detection.nearend_average_blocks ==
             c2.suppressor.subband_nearend_detection.nearend_average_blocks &&
         c1.suppressor.subband_nearend_detection.subband1.low ==
             c2.suppressor.subband_nearend_detection.subband1.low &&
         c1.suppressor.subband_nearend_detection.subband1.high ==
             c2.suppressor.subband_nearend_detection.subband1.high &&
         c1.suppressor.subband_nearend_detection.subband2.low ==
             c2.suppressor.subband_nearend_detection.subband2.low &&
         c1.suppressor.subband_nearend_detection.subband2.high ==
             c2.suppressor.subband_nearend_detection.subband2.high &&
         c1.suppressor.use_subband_nearend_detection ==
             c2.suppressor.use_subband_nearend_detection &&
         c1.suppressor.high_bands_suppression.enr_threshold ==
             c2.suppressor.high_bands_suppression.enr_threshold &&
         c1.suppressor.high_bands_suppression.max_gain_during_echo ==
             c2.suppressor.high_bands_suppression.max_gain_during_echo &&
         c1.suppressor.high_bands_suppression
                 .anti_howling_activation_threshold ==
             c2.suppressor.high_bands_suppression
                 .anti_howling_activation_threshold &&
         c1.suppressor.high_bands_suppression.anti_howling_gain ==
             c2.suppressor.high_bands_suppression.anti_howling_gain &&
         c1.suppressor.floor_first_increase ==
             c2.suppressor.floor_first_increase &&
         c1.suppressor.conservative_hf_suppression ==
             c2.suppressor.conservative_hf_suppression &&
         c1.multi_channel.detect_stereo_content ==
             c2.multi_channel.detect_stereo_content &&
         c1.multi_channel.stereo_detection_threshold ==
             c2.multi_channel.stereo_detection_threshold &&
         c1.multi_channel.stereo_detection_timeout_threshold_seconds ==
             c2.multi_channel.stereo_detection_timeout_threshold_seconds &&
         c1.multi_channel.stereo_detection_hysteresis_seconds ==
             c2.multi_channel.stereo_detection_hysteresis_seconds;
}
}  // namespace

TEST(EchoCanceller3Config, ValidConfigIsNotModified) {
  EchoCanceller3Config config;
  EXPECT_TRUE(EchoCanceller3Config::Validate(&config));
  EchoCanceller3Config default_config;
  EXPECT_TRUE(config == default_config);
}

TEST(EchoCanceller3Config, InvalidConfigIsCorrected) {
  // Change a parameter and validate.
  EchoCanceller3Config config;
  config.echo_model.min_noise_floor_power = -1600000.f;
  EXPECT_FALSE(EchoCanceller3Config::Validate(&config));
  EXPECT_GE(config.echo_model.min_noise_floor_power, 0.f);
  // Verify remaining parameters are unchanged.
  EchoCanceller3Config default_config;
  config.echo_model.min_noise_floor_power =
      default_config.echo_model.min_noise_floor_power;
  EXPECT_TRUE(config == default_config);
}

TEST(EchoCanceller3Config, ValidatedConfigsAreValid) {
  EchoCanceller3Config config;
  config.delay.down_sampling_factor = 983;
  EXPECT_FALSE(EchoCanceller3Config::Validate(&config));
  EXPECT_TRUE(EchoCanceller3Config::Validate(&config));
}
}  // namespace webrtc
