/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/stereo/include/stereo_encoder_adapter.h"

#include "common_video/include/video_frame.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/engine/scopedvideoencoder.h"
#include "media/engine/videoencodersoftwarefallbackwrapper.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"

#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AppRTCMobile", __VA_ARGS__)
#include "sdk/android/src/jni/native_handle_impl.h"
#include "media/base/h264_profile_level_id.h"
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "modules/video_coding/codecs/vp8/temporal_layers.h"


namespace webrtc {

class StereoEncoderAdapter::AdapterEncodedImageCallback
    : public webrtc::EncodedImageCallback {
 public:
  AdapterEncodedImageCallback(webrtc::StereoEncoderAdapter* adapter,
                              StereoCodecStream stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override {
    if (!adapter_)
      return Result(Result::OK);
    return adapter_->OnEncodedImage(stream_idx_, encoded_image,
                                    codec_specific_info, fragmentation);
  }

 private:
  StereoEncoderAdapter* adapter_;
  const StereoCodecStream stream_idx_;
};

struct StereoEncoderAdapter::EncodedImageData {
  explicit EncodedImageData(StereoCodecStream stream_idx)
      : stream_idx_(stream_idx) {
    RTC_DCHECK_EQ(kAXXStream, stream_idx);
    encodedImage_._length = 0;
  }
  EncodedImageData(StereoCodecStream stream_idx,
                   const EncodedImage& encodedImage,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const RTPFragmentationHeader* fragmentation)
      : stream_idx_(stream_idx),
        encodedImage_(encodedImage),
        codecSpecificInfo_(*codecSpecificInfo) {
    fragmentation_.CopyFrom(*fragmentation);
  }
  const StereoCodecStream stream_idx_;
  EncodedImage encodedImage_;
  const CodecSpecificInfo codecSpecificInfo_;
  RTPFragmentationHeader fragmentation_;

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EncodedImageData);
};

StereoEncoderAdapter::StereoEncoderAdapter(
    cricket::WebRtcVideoEncoderFactory* ex_factory, 
    cricket::WebRtcVideoEncoderFactory* in_factory)
    : factory_(ex_factory), 
      software_factory_(in_factory),
      encoded_complete_callback_(nullptr),
      clock_(Clock::GetRealTimeClock()) {}

StereoEncoderAdapter::StereoEncoderAdapter(
    cricket::WebRtcVideoEncoderFactory* factory)
    : factory_(factory), 
      software_factory_(factory),
      encoded_complete_callback_(nullptr),
      clock_(Clock::GetRealTimeClock()) {}


StereoEncoderAdapter::~StereoEncoderAdapter() {
  Release();
}

int StereoEncoderAdapter::InitEncode(const VideoCodec* inst,
                                     int number_of_cores,
                                     size_t max_payload_size) {
  counter_[0]=counter_[1]=0;
  ALOGE("Qiang Chen StereoEncoder::InitEncode %s %d %d %d %d", inst->plName, inst->plType, inst->codecType, inst->VP8().complexity, inst->VP8().keyFrameInterval);
  const size_t buffer_size =
      CalcBufferSize(VideoType::kI420, inst->width, inst->height);
  stereo_dummy_planes_.resize(buffer_size);
  // It is more expensive to encode 0x00, so use 0x80 instead.
  std::fill(stereo_dummy_planes_.begin(), stereo_dummy_planes_.end(), 0x80);

  cricket::VideoCodec codec("VP8");
  
  std::unique_ptr<VideoEncoder> external_encoder =
      CreateScopedVideoEncoder(factory_, codec);
  external_encoder->InitEncode(inst, number_of_cores, max_payload_size);
  adapter_callbacks_.emplace_back(new AdapterEncodedImageCallback(
      this, static_cast<StereoCodecStream>(0)));
  external_encoder->RegisterEncodeCompleteCallback(adapter_callbacks_.back().get());
  encoders_.push_back(std::move(external_encoder));

  std::unique_ptr<VideoEncoder> software_encoder = 
      CreateScopedVideoEncoder(software_factory_, codec);
  software_encoder->InitEncode(inst, number_of_cores, max_payload_size);
  adapter_callbacks_.emplace_back(new AdapterEncodedImageCallback(
      this, static_cast<StereoCodecStream>(1)));
  software_encoder->RegisterEncodeCompleteCallback(adapter_callbacks_.back().get());
  encoders_.push_back(std::move(software_encoder));

  //factory_ = nullptr;
  //software_factory_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

bool StereoEncoderAdapter::SupportsNativeHandle() const {
  return true;
}

int StereoEncoderAdapter::Encode(const VideoFrame& input_image,
                                 const CodecSpecificInfo* codec_specific_info,
                                 const std::vector<FrameType>* frame_types) {
  //int64_t l_time = clock_->TimeInMilliseconds();
   counter_[2]++;
  if (counter_[2]%100==0) {
     int64_t c_time = clock_->TimeInMilliseconds();
     ALOGE("Qiang Chen Stereo Encoder Incoming Encode FPS: %lld", 100*1000/(c_time - l_time_[2]));
     l_time_[2] = c_time;
  }
  if (!encoded_complete_callback_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  rtc::scoped_refptr<VideoFrameBuffer> mask_buffer = input_image.video_frame_buffer()->SpawnMask();

  // Encode AXX
  if (mask_buffer) {
    VideoFrame alpha_image(mask_buffer, input_image.timestamp(),
                           input_image.render_time_ms(),
                            input_image.rotation());
    frame_count_.emplace(input_image.timestamp(), 2);

    //ALOGE("Qiang Chen StereoEncoder Alpha Encode %d", alpha_image.timestamp());
    encoders_[kAXXStream]->Encode(alpha_image, codec_specific_info,
                                  frame_types);
    //ALOGE("Qiang Chen StereoEncoder Alpha Encode End");
    //ALOGE("Qiang Chen StereoEncoder YUV Encode %d", input_image.timestamp());
    encoders_[kYUVStream]->Encode(input_image, codec_specific_info,
                                  frame_types);
    //ALOGE("Qiang Chen StereoEncoder YUV Encode End");
  } else {
    /*rtc::scoped_refptr<I420BufferInterface> yuva_buffer =
        input_image.video_frame_buffer()->ToI420();
    if (yuva_buffer->HasAlpha()) {
      rtc::scoped_refptr<WrappedI420Buffer> alpha_buffer(
  	new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
          input_image.width(), input_image.height(), yuva_buffer->DataA(),
          yuva_buffer->StrideA(), stereo_dummy_planes_.data(),
          yuva_buffer->StrideU(), stereo_dummy_planes_.data(),
          yuva_buffer->StrideV(),
          rtc::KeepRefUntilDone(input_image.video_frame_buffer())));
      VideoFrame alpha_image(alpha_buffer, input_image.timestamp(),
  	                     input_image.render_time_ms(),
  	                     input_image.rotation());
      yuv_image = VideoFrame(yuva_buffer,  
                           input_image.timestamp(),
  	                   input_image.render_time_ms(),
  	                   input_image.rotation());
      encoders_[kAXXStream]->Encode(alpha_image, codec_specific_info,
	                            frame_types);
      frame_count_.emplace(input_image.timestamp(), 2);
    } else*/
    RTC_DCHECK(frame_count_.find(input_image.timestamp()) ==
	     frame_count_.end());
    frame_count_.emplace(input_image.timestamp(), 1);
    encoders_[kYUVStream]->Encode(input_image, codec_specific_info,
                                         frame_types);    
  }

  //int64_t c_time = clock_->TimeInMilliseconds();
  //ALOGE("Qiang Chen StereoEncode Time = %lld ms", c_time-l_time);  
  
  return 0;
}

int StereoEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::SetChannelParameters(uint32_t packet_loss,
                                               int64_t rtt) {
  for (auto& encoder : encoders_) {
    const int rv = encoder->SetChannelParameters(packet_loss, rtt);
    if (rv)
      return rv;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::SetRateAllocation(const BitrateAllocation& bitrate,
                                            uint32_t new_framerate) {  
  //for (auto& encoder : encoders_) {
  encoders_[kYUVStream]->SetRateAllocation(bitrate, new_framerate);
  encoders_[kAXXStream]->SetRateAllocation(bitrate, new_framerate * 2);
  //  if (rv)
  //    return rv;
  //}
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::Release() {
  for (auto& encoder : encoders_) {
    const int rv = encoder->Release();
    if (rv)
      return rv;
    // factory_->DestroyVideoEncoder(encoder.get());
  }
  encoders_.clear();
  adapter_callbacks_.clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

EncodedImageCallback::Result StereoEncoderAdapter::OnEncodedImage(
    StereoCodecStream stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  //ALOGE("Qiang Chen StereoEncoder %s Encoded %d", stream_idx == kYUVStream? "YUV":"Alpha", encodedImage._timeStamp);

  const auto& frame_count_object = frame_count_.find(encodedImage._timeStamp);
  counter_[stream_idx]++;
  if (counter_[stream_idx]%100==0) {
     int64_t c_time = clock_->TimeInMilliseconds();
     int64_t l_time = l_time_[stream_idx];
     l_time_[stream_idx] = c_time; 
     ALOGE("Qiang Chen FPS: %s %lld", stream_idx == kYUVStream? "YUV":"Alpha", 100*1000/(c_time-l_time));
  }

  // If the timestamp has already be deleted, this means the frame
  // arrives later than its future frame, but we still send it out
  // not to break the frame dependence chain on the receiver side.
  int frame_count = frame_count_object != frame_count_.end()
                        ? frame_count_object->second
                        : kStereoCodecStreams;

  frame_count_.erase(frame_count_.begin(), frame_count_object);

  CodecSpecificInfo* yuv_codec =
      const_cast<CodecSpecificInfo*>(codecSpecificInfo);
  yuv_codec->codecType = kVideoCodecStereo;
  yuv_codec->codec_name = "stereo-xxx";
  yuv_codec->stereoInfo.stereoCodecType = kVideoCodecVP8;
  yuv_codec->stereoInfo.frameIndex = stream_idx;
  yuv_codec->stereoInfo.frameCount = frame_count;
  yuv_codec->stereoInfo.pictureIndex = ++picture_index_;
  //if (stream_idx != kAXXStream)
  encoded_complete_callback_->OnEncodedImage(encodedImage, yuv_codec,
                                             fragmentation);
  return EncodedImageCallback::Result(EncodedImageCallback::Result::OK);
}

}  // namespace webrtc
