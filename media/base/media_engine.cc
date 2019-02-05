/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_engine.h"

#include <stddef.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "api/video/video_bitrate_allocation.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_encode.h"

namespace cricket {

RtpCapabilities::RtpCapabilities() = default;
RtpCapabilities::~RtpCapabilities() = default;

bool RtpCapabilities::AddRtpExtension(const std::string& uri) {
  return AddRtpExtension(uri, false);
}

bool RtpCapabilities::AddRtpExtension(const std::string& uri, bool encrypt) {
  std::vector<int> used_ids;
  used_ids.reserve(header_extensions.size());

  for (const webrtc::RtpExtension& header_extension : header_extensions) {
    if (header_extension.uri == uri) {
      // URI already registered.
      return header_extension.encrypt == encrypt;
    }
    used_ids.push_back(header_extension.id);
  }

  std::sort(used_ids.begin(), used_ids.end());

  // Find the lowest available ID.
  int id = 1;
  for (size_t i = 0; i < used_ids.size(); ++i, ++id) {
    // RTC_DCHECKs demonstrate that used IDs are strictly increasing,
    // beginning with 1, and therefore if there is a gap, it will be found,
    // and if there is no gap, |id| will be set to used_ids.size() at the end.
    RTC_DCHECK(i + 1 == used_ids.size() || used_ids[i] != used_ids[i + 1]);
    RTC_DCHECK_LE(id, used_ids[i]);
    if (id < used_ids[i]) {
      break;
    }
  }

  header_extensions.emplace_back(uri, id, encrypt);

  return true;
}

webrtc::RtpParameters CreateRtpParametersWithOneEncoding() {
  webrtc::RtpParameters parameters;
  webrtc::RtpEncodingParameters encoding;
  parameters.encodings.push_back(encoding);
  return parameters;
}

webrtc::RtpParameters CreateRtpParametersWithEncodings(StreamParams sp) {
  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  size_t encoding_count = primary_ssrcs.size();

  std::vector<webrtc::RtpEncodingParameters> encodings(encoding_count);
  for (size_t i = 0; i < encodings.size(); ++i) {
    encodings[i].ssrc = primary_ssrcs[i];
  }
  webrtc::RtpParameters parameters;
  parameters.encodings = encodings;
  parameters.rtcp.cname = sp.cname;
  return parameters;
}

webrtc::RTCError CheckRtpParametersValues(
    const webrtc::RtpParameters& rtp_parameters) {
  using webrtc::RTCErrorType;

  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].bitrate_priority <= 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                           "Attempted to set RtpParameters bitrate_priority to "
                           "an invalid number. bitrate_priority must be > 0.");
    }
    if (rtp_parameters.encodings[i].scale_resolution_down_by &&
        *rtp_parameters.encodings[i].scale_resolution_down_by < 1.0) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_RANGE,
          "Attempted to set RtpParameters scale_resolution_down_by to an "
          "invalid number. scale_resolution_down_by must be >= 1.0");
    }
    if (rtp_parameters.encodings[i].min_bitrate_bps &&
        rtp_parameters.encodings[i].max_bitrate_bps) {
      if (*rtp_parameters.encodings[i].max_bitrate_bps <
          *rtp_parameters.encodings[i].min_bitrate_bps) {
        LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters min bitrate "
                             "larger than max bitrate.");
      }
    }
    if (rtp_parameters.encodings[i].num_temporal_layers) {
      if (*rtp_parameters.encodings[i].num_temporal_layers < 1 ||
          *rtp_parameters.encodings[i].num_temporal_layers >
              webrtc::kMaxTemporalStreams) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters "
                             "num_temporal_layers to an invalid number.");
      }
    }
    if (i > 0 && (rtp_parameters.encodings[i].num_temporal_layers !=
                  rtp_parameters.encodings[i - 1].num_temporal_layers)) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_MODIFICATION,
          "Attempted to set RtpParameters num_temporal_layers "
          "at encoding layer i: " +
              rtc::ToString(i) +
              " to a different value than other encoding layers.");
    }
  }

  return webrtc::RTCError::OK();
}

webrtc::RTCError CheckRtpParametersInvalidModificationAndValues(
    const webrtc::RtpParameters& old_rtp_parameters,
    const webrtc::RtpParameters& rtp_parameters) {
  using webrtc::RTCErrorType;
  if (rtp_parameters.encodings.size() != old_rtp_parameters.encodings.size()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with different encoding count");
  }
  if (rtp_parameters.rtcp != old_rtp_parameters.rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified RTCP parameters");
  }
  if (rtp_parameters.header_extensions !=
      old_rtp_parameters.header_extensions) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified header extensions");
  }

  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].ssrc !=
        old_rtp_parameters.encodings[i].ssrc) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                           "Attempted to set RtpParameters with modified SSRC");
    }
  }

  return CheckRtpParametersValues(rtp_parameters);
}

CompositeMediaEngine::CompositeMediaEngine(
    std::unique_ptr<VoiceEngineInterface> voice_engine,
    std::unique_ptr<VideoEngineInterface> video_engine)
    : voice_engine_(std::move(voice_engine)),
      video_engine_(std::move(video_engine)) {}

CompositeMediaEngine::~CompositeMediaEngine() = default;

bool CompositeMediaEngine::Init() {
  voice().Init();
  return true;
}

VoiceEngineInterface& CompositeMediaEngine::voice() {
  return *voice_engine_.get();
}

VideoEngineInterface& CompositeMediaEngine::video() {
  return *video_engine_.get();
}

const VoiceEngineInterface& CompositeMediaEngine::voice() const {
  return *voice_engine_.get();
}

const VideoEngineInterface& CompositeMediaEngine::video() const {
  return *video_engine_.get();
}

};  // namespace cricket
