/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_VIDEO_RECEIVER2_H_
#define MODULES_VIDEO_CODING_VIDEO_RECEIVER2_H_

#include "modules/video_coding/decoder_database.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/generic_decoder.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/one_time_event.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

// This class is a copy of vcm::VideoReceiver, trimmed down to what's used by
// VideoReceive stream, with the aim to incrementally trim it down further and
// ultimately delete it. It's difficult to do this incrementally with the
// original VideoReceiver class, since it is used by the legacy
// VideoCodingModule api.
class VideoReceiver2 {
 public:
  VideoReceiver2(Clock* clock, VCMTiming* timing);
  ~VideoReceiver2();

  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame);

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType);
  int32_t RegisterReceiveCallback(VCMReceiveCallback* receiveCallback);
  int32_t RegisterFrameTypeCallback(VCMFrameTypeCallback* frameTypeCallback);
  int32_t RegisterPacketRequestCallback(VCMPacketRequestCallback* callback);

  int32_t Decode(const webrtc::VCMEncodedFrame* frame);

  void TriggerDecoderShutdown();

  // Notification methods that are used to check our internal state and validate
  // threading assumptions. These are called by VideoReceiveStream.
  // See |IsDecoderThreadRunning()| for more details.
  void DecoderThreadStarting();
  void DecoderThreadStopped();

 protected:
  int32_t Decode(const webrtc::VCMEncodedFrame& frame);
  int32_t RequestKeyFrame();

 private:
  // Used for DCHECKing thread correctness.
  // In build where DCHECKs are enabled, will return false before
  // DecoderThreadStarting is called, then true until DecoderThreadStopped
  // is called.
  // In builds where DCHECKs aren't enabled, it will return true.
  bool IsDecoderThreadRunning();

  rtc::ThreadChecker construction_thread_checker_;
  rtc::ThreadChecker decoder_thread_checker_;
  rtc::ThreadChecker module_thread_checker_;
  Clock* const clock_;
  rtc::CriticalSection process_crit_;
  VCMTiming* _timing;
  VCMDecodedFrameCallback _decodedFrameCallback;

  // These callbacks are set on the construction thread before being attached
  // to the module thread or decoding started, so a lock is not required.
  VCMFrameTypeCallback* _frameTypeCallback;
  VCMPacketRequestCallback* _packetRequestCallback;

  // Used on both the module and decoder thread.
  bool _scheduleKeyRequest RTC_GUARDED_BY(process_crit_);
  bool drop_frames_until_keyframe_ RTC_GUARDED_BY(process_crit_);

  // Modified on the construction thread while not attached to the process
  // thread.  Once attached to the process thread, its value is only read
  // so a lock is not required.
  size_t max_nack_list_size_;

  // Callbacks are set before the decoder thread starts.
  // Once the decoder thread has been started, usage of |_codecDataBase| moves
  // over to the decoder thread.
  VCMDecoderDataBase _codecDataBase;

  ThreadUnsafeOneTimeEvent first_frame_received_
      RTC_GUARDED_BY(decoder_thread_checker_);
#if RTC_DCHECK_IS_ON
  bool decoder_thread_is_running_ = false;
#endif
};

}  // namespace vcm
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_VIDEO_RECEIVER2_H_
