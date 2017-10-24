/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_

#if defined(WEBRTC_WIN)
// This is a workaround on Windows due to the fact that some Windows
// headers define CreateEvent as a macro to either CreateEventW or CreateEventA.
// This can cause problems since we use that name as well and could
// declare them as one thing here whereas in another place a windows header
// may have been included and then implementing CreateEvent() causes compilation
// errors.  So for consistency, we include the main windows header here.
#include <windows.h>
#endif

#include "api/video/video_frame.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "system_wrappers/include/event_wrapper.h"

namespace webrtc {

class Clock;
class EncodedImageCallback;
// TODO(pbos): Remove VCMQMSettingsCallback completely. This might be done by
// removing the VCM and use VideoSender/VideoReceiver as a public interface
// directly.
class VCMQMSettingsCallback;
class VideoBitrateAllocator;
class VideoEncoder;
class VideoDecoder;
struct CodecSpecificInfo;

class EventFactory {
 public:
  virtual ~EventFactory() {}

  virtual EventWrapper* CreateEvent() = 0;
};

class EventFactoryImpl : public EventFactory {
 public:
  virtual ~EventFactoryImpl() {}

  virtual EventWrapper* CreateEvent() { return EventWrapper::Create(); }
};

// Used to indicate which decode with errors mode should be used.
enum VCMDecodeErrorMode {
  kNoErrors,         // Never decode with errors. Video will freeze
                     // if nack is disabled.
  kSelectiveErrors,  // Frames that are determined decodable in
                     // VCMSessionInfo may be decoded with missing
                     // packets. As not all incomplete frames will be
                     // decodable, video will freeze if nack is disabled.
  kWithErrors        // Release frames as needed. Errors may be
                     // introduced as some encoded frames may not be
                     // complete.
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
