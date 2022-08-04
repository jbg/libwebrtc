/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_DMABUF_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_DMABUF_DESKTOP_FRAME_H_

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// DesktopFrame implementation used by PipeWire based capturer
// for DMA-BUFs to avoid copy operation and just take ownership
// of the passed image_data.
class DmaBufDesktopFrame final : public DesktopFrame {
 public:
  // The "updated_image_data" is updated pointer to the correct
  // position in the "image_data" frame, because there might be
  // an offset, based on the compositor implementation.
  DmaBufDesktopFrame(DesktopSize size,
                     int stride,
                     std::unique_ptr<uint8_t[]> image_data,
                     uint8_t* updated_image_data);

  DmaBufDesktopFrame(const DmaBufDesktopFrame&) = delete;
  DmaBufDesktopFrame& operator=(const DmaBufDesktopFrame&) = delete;

  ~DmaBufDesktopFrame() override;

 private:
  std::unique_ptr<uint8_t[]> image_data_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_DMABUF_DESKTOP_FRAME_H_
