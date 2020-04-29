/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_OBSERVER_H_
#define API_VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_OBSERVER_H_

#include <string>
#include <vector>

#include "api/video/video_stream_encoder_observer.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoStreamEncoderObserver : public VideoStreamEncoderObserver {
 public:
  MOCK_METHOD2(OnEncodedFrameTimeMeasured, void(int, int));
  MOCK_METHOD2(OnIncomingFrame, void(int, int));
  MOCK_METHOD2(OnSendEncodedImage,
               void(const EncodedImage&, const CodecSpecificInfo*));
  MOCK_METHOD1(OnEncoderImplementationChanged, void(const std::string&));
  MOCK_METHOD1(OnFrameDropped, void(DropReason));
  MOCK_METHOD2(OnEncoderReconfigured,
               void(const VideoEncoderConfig&,
                    const std::vector<VideoStream>&));
  MOCK_METHOD3(OnAdaptationChanged,
               void(VideoAdaptationReason,
                    const VideoAdaptationCounters&,
                    const VideoAdaptationCounters&));
  MOCK_METHOD0(ClearAdaptationStats, void());
  MOCK_METHOD2(UpdateAdaptationSettings,
               void(AdaptationSettings, AdaptationSettings));
  MOCK_METHOD0(OnMinPixelLimitReached, void());
  MOCK_METHOD0(OnInitialQualityResolutionAdaptDown, void());
  MOCK_METHOD1(OnSuspendChange, void(bool));
  MOCK_METHOD2(OnBitrateAllocationUpdated,
               void(const VideoCodec&, const VideoBitrateAllocation&));
  MOCK_METHOD1(OnEncoderInternalScalerUpdate, void(bool));
  MOCK_CONST_METHOD0(GetInputFrameRate, int());
};

}  // namespace webrtc

#endif  // API_VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_OBSERVER_H_
