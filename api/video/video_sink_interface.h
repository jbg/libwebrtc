/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_SINK_INTERFACE_H_
#define API_VIDEO_VIDEO_SINK_INTERFACE_H_

#include "rtc_base/checks.h"

namespace webrtc {
class TaskQueueBase;
}

namespace rtc {

template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual ~VideoSinkInterface() = default;

  // Called when frames will start to arrive. The `delivery_queue` will be the
  // task queue on which the OnFrame and OnDiscardedFrame calls will be made.
  // NOTE: Not all implementations call OnStart/OnStop right now, so if you find
  // that calls are missing, please fix that area of the code.
  virtual void OnStart(webrtc::TaskQueueBase* delivery_queue) {}

  // Called when frame delivery has been stopped and further calls to On*Frame
  // methods will not be made.
  virtual void OnStop() {}

  virtual void OnFrame(const VideoFrameT& frame) = 0;

  // Should be called by the source when it discards the frame due to rate
  // limiting.
  virtual void OnDiscardedFrame() {}
};

}  // namespace rtc

#endif  // API_VIDEO_VIDEO_SINK_INTERFACE_H_
