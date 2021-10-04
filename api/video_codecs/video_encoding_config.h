/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODING_CONFIG_H_
#define API_VIDEO_CODECS_VIDEO_ENCODING_CONFIG_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/render_resolution.h"
#include "api/video/video_codec_type.h"

namespace webrtc {

// TODO(before_commit): move this helper to shared place (video_codec_type.h ?)
RTC_EXPORT VideoCodecType PayloadStringToCodecType(absl::string_view name);

class VideoEncodingParameters {
 public:
  VideoEncodingParameters() = default;
  VideoEncodingParameters(const VideoEncodingParameters&) = default;
  VideoEncodingParameters& operator=(const VideoEncodingParameters&) = default;
  ~VideoEncodingParameters() = default;

  absl::string_view codec_name() const { return codec_name_; }
  void set_codec_name(absl::string_view codec_name) {
    codec_name_ = std::string(codec_name);
    codec_type_ = PayloadStringToCodecType(codec_name);
  }

  VideoCodecType codec_type() const { return codec_type_; }

  RenderResolution render_resolution() const { return resolution_; }
  void set_render_resolution(RenderResolution value) { resolution_ = value; }

  // Scalability mode as described in
  // https://www.w3.org/TR/webrtc-svc/#scalabilitymodes*
  // or value 'NONE' to indicate no scalability.
  absl::string_view ScalabilityMode() const { return scalability_mode_; }
  void SetScalabilityMode(absl::string_view scalability_mode) {
    scalability_mode_ = std::string(scalability_mode);
  }

  absl::optional<DataRate> min_bitrate() const { return min_bitrate_; }
  absl::optional<DataRate> max_bitrate() const { return max_bitrate_; }
  absl::optional<DataRate> start_bitrate() const { return start_bitrate_; }

  // TODO: validate min <= start <= max for bitrates that are set.
  void set_min_bitrate(DataRate value) { min_bitrate_ = value; }
  void set_max_bitrate(DataRate value) { max_bitrate_ = value; }
  void set_start_bitrate(DataRate value) { start_bitrate_ = value; }

  Frequency max_framerate() const { return max_framerate_; }
  void set_max_framerate(Frequency value) { max_framerate_ = value; }

  // ???
  bool automatic_resize_on() const { return automatic_resize_on_; }

  // Derivatives.
  int num_temporal_layers() const;
  int num_spatial_layers() const;

 private:
  std::string codec_name_;
  VideoCodecType codec_type_ = kVideoCodecGeneric;
  std::string scalability_mode_;
  RenderResolution resolution_;
  absl::optional<DataRate> min_bitrate_;
  absl::optional<DataRate> max_bitrate_;
  absl::optional<DataRate> start_bitrate_;
  Frequency max_framerate_ = Frequency::Hertz(30);
  bool automatic_resize_on_ = false;
};

class VideoStreamTrackParameters {
 public:
  VideoStreamTrackParameters() : encodings_(1) {}

  rtc::ArrayView<const VideoEncodingParameters> encodings() const {
    return encodings_;
  }
  rtc::ArrayView<VideoEncodingParameters> encodings() { return encodings_; }

  void set_num_simulcast_layers(size_t value) {
    RTC_DCHECK_GT(value, 0);
    encodings_.resize(value);
  }
  int num_simulcast_layers() const { return encodings_.size(); }
  bool simulcast() const { return encodings_.size() > 1; }

  VideoCodecType codec_type() const {
    VideoCodecType type = encodings_.front().codec_type();
    for (size_t i = 1; i < encodings_.size(); ++i) {
      if (type != encodings_[i].codec_type()) {
        return kVideoCodecMultiplex;
      }
    }
    return type;
  }

  RenderResolution max_resolution() const {
    int max_width = 0;
    int max_height = 0;
    for (const auto& encoding : encodings_) {
      const auto& r = encoding.render_resolution();
      if (r.Width() > max_width)
        max_width = r.Width();
      if (r.Height() > max_height)
        max_height = r.Height();
    }
    return RenderResolution(max_width, max_height);
  }

  int num_temporal_layers() const {
    int ret = 1;
    for (const auto& encoding : encodings_) {
      ret = std::max(ret, encoding.num_temporal_layers());
    }
    return ret;
  }

 private:
  std::vector<VideoEncodingParameters> encodings_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODING_CONFIG_H_
