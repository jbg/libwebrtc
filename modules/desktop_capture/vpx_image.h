/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_VPX_IMAGE_H_
#define MODULES_DESKTOP_CAPTURE_VPX_IMAGE_H_

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "vpx/vpx_image.h"

namespace webrtc {

// A wrapper of <vpx_image, uint8_t[]> to help draw DesktopFrames on to it and
// reduce the complexity of using DesktopFrame with VPx encoders as well as
// other encoders which accept only YUV input.
class VpxImage final {
 public:
  // If |use_i444| is false, I420 will be used.
  VpxImage(const DesktopSize& size, bool use_i444);
  ~VpxImage();

  const vpx_image_t& image() const;
  DesktopSize size() const;

  // Draws |frame| onto current vpx_image_t. This function asserts that
  // |frame|.size() is equal to size().
  void Draw(const DesktopFrame& frame);

  // Draws |image_| back on to |frame| from (0, 0). This function sets only the
  // RGBA bytes of |frame|, other properties such as the updated region, dpi,
  // capture_time_ms, etc, are kept unchanged. This function asserts that
  // |frame|.size() is larger than or equal to size().
  // Based on the chroma sampling method selected (I444 v.s. I420), the output
  // image is not guaranteed to perfectly match the input frame of Draw().
  void Export(DesktopFrame* frame) const;

 private:
  // Draws |rect| subregion of |frame| onto current vpx_image_t.
  void DrawRect(const DesktopFrame& frame, const DesktopRect& rect);

  vpx_image_t image_;
  std::unique_ptr<uint8_t[]> buffer_;
  // We should draw the entire |frame| if it's the first frame rather than
  // respecting DesktopFrame::updated_region().
  bool first_frame_ = true;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_VPX_IMAGE_H_
