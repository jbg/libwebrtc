/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/vpx_image.h"

#include "modules/desktop_capture/desktop_frame_generator.h"
#include "test/gtest.h"

namespace webrtc {

class VpxImageTest : public testing::Test,
                     public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_CASE_P(I444, VpxImageTest, ::testing::Values(true));
INSTANTIATE_TEST_CASE_P(I420, VpxImageTest, ::testing::Values(false));

TEST_P(VpxImageTest, FullyDrawTheFirstImage) {
  const DesktopSize frame_size(100, 80);
  VpxImage image(frame_size, GetParam());

  ColorfulDesktopFramePainter painter;
  PainterDesktopFrameGenerator generator;
  *generator.size() = frame_size;
  generator.set_desktop_frame_painter(&painter);
  painter.colors()->push_back(RgbaColor(0xFF, 0, 0));
  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));

  auto frame = generator.GetNextFrame(nullptr);
  image.Draw(*frame);

  BasicDesktopFrame output(frame_size);
}

}  // namespace webrtc
