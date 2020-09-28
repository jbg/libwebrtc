#ifndef VIDEO_STREAM_FROM_FILE_ENCODER_H_
#define VIDEO_STREAM_FROM_FILE_ENCODER_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/ivf_file_reader.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class StreamFromFileEncoder : public VideoEncoder {
 public:
  static std::unique_ptr<VideoEncoder> Create(std::string filename);

  explicit StreamFromFileEncoder(std::string filename);
  virtual ~StreamFromFileEncoder() = default;

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;

  virtual void SetRates(const RateControlParameters&) override;
  int GetConfiguredInputFramerate() const;
  EncoderInfo GetEncoderInfo() const override;

  static const char* kImplementationName;

 protected:
  std::unique_ptr<IvfFileReader> ivf_file_reader_;
  absl::optional<uint64_t> last_timestamp_;
  uint64_t average_time_delta_ = 0;
  VideoCodec config_ RTC_GUARDED_BY(crit_sect_);
  EncodedImageCallback* callback_ RTC_GUARDED_BY(crit_sect_);
  VideoBitrateAllocation target_bitrate_ RTC_GUARDED_BY(crit_sect_);
  int configured_input_framerate_ RTC_GUARDED_BY(crit_sect_);
  int max_target_bitrate_kbps_ RTC_GUARDED_BY(crit_sect_);
  bool pending_keyframe_ RTC_GUARDED_BY(crit_sect_);
  int counter_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;
};

}  // namespace webrtc

#endif  // VIDEO_STREAM_FROM_FILE_ENCODER_H_
