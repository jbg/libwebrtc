/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_
#define API_VIDEO_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_

#include <memory>
#include <utility>
#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// A factory that creates VideoBitrateAllocator.
// NOTE: This class is still under development and may change without notice.
class VideoBitrateAllocatorFactory {
 public:
  virtual ~VideoBitrateAllocatorFactory() = default;
  // Creates a VideoBitrateAllocator for a specific video codec.
  virtual std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
      const VideoCodec& codec) = 0;
};

// A handle so that we can use rtc::scoped_refptr on PeerConnectionDependencies
// without having to modify the abstract VideoBitrateAllocatorFactory.
struct VideoBitrateAllocatorFactoryHandle : public rtc::RefCountInterface {
  VideoBitrateAllocatorFactoryHandle(
      std::unique_ptr<VideoBitrateAllocatorFactory> factory_arg)
      : factory(std::move(factory_arg)) {}
  std::unique_ptr<VideoBitrateAllocatorFactory> factory;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_
