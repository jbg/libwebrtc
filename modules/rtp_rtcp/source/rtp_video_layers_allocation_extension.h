#ifndef THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYER_ALLOCATION_EXTENSION_H_
#define THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYER_ALLOCATION_EXTENSION_H_

#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_layers_allocation.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtpVideoLayersAllocationExtension {
 public:
  using value_type = VideoLayersAllocation;
  static constexpr RTPExtensionType kId = kRtpExtensionVideoLayersAllocation;
  static constexpr const char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation";
  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    VideoLayersAllocation* allocation);
  static size_t ValueSize(const VideoLayersAllocation& allocation);
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const VideoLayersAllocation& allocation);

 private:
  // Retruns size of the encoded extension.
  // If |buffer| != nullptr, writes upto |size| bytes. Returns 0 if there's not
  // enough bytes in the buffer.
  static size_t Encode(uint8_t* buffer,
                       const VideoLayersAllocation& allocation,
                       size_t buffer_size);
};

}  // namespace webrtc
#endif  // THIRD_PARTY_WEBRTC_FILES_STABLE_WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYER_ALLOCATION_EXTENSION_H_
