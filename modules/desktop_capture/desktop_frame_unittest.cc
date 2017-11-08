/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_frame.h"

#include "modules/desktop_capture/desktop_frame_generator.h"
#include "test/gtest.h"

namespace webrtc {

TEST(DesktopFrameTest, EqualTo_AnotherFrameWithSameSize) {
  const DesktopSize frame_size(100, 80);
  BlackWhiteDesktopFramePainter painter;
  PainterDesktopFrameGenerator generator;
  *generator.size() = frame_size;
  generator.set_desktop_frame_painter(&painter);

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto first_frame = generator.GetNextFrame(nullptr);

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto second_frame = generator.GetNextFrame(nullptr);

  ASSERT_TRUE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 50, 30)));

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 50, 30));
  second_frame = generator.GetNextFrame(nullptr);

  ASSERT_FALSE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_FALSE(first_frame->EqualTo(*second_frame,
                                    DesktopRect::MakeXYWH(10, 10, 50, 30)));
}

TEST(DesktopFrameTest, EqualTo_AnotherFrameWithSmallerSize) {
  BlackWhiteDesktopFramePainter painter;
  PainterDesktopFrameGenerator generator;
  generator.size()->set(100, 80);
  generator.set_desktop_frame_painter(&painter);

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto first_frame = generator.GetNextFrame(nullptr);

  generator.size()->set(90, 70);
  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto second_frame = generator.GetNextFrame(nullptr);

  ASSERT_FALSE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 50, 30)));

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 50, 30));
  second_frame = generator.GetNextFrame(nullptr);

  ASSERT_FALSE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_FALSE(first_frame->EqualTo(*second_frame,
                                    DesktopRect::MakeXYWH(10, 10, 50, 30)));
}

TEST(DesktopFrameTest, EqualTo_AnotherFrameWithLargerSize) {
  BlackWhiteDesktopFramePainter painter;
  PainterDesktopFrameGenerator generator;
  generator.size()->set(90, 70);
  generator.set_desktop_frame_painter(&painter);

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto first_frame = generator.GetNextFrame(nullptr);

  generator.size()->set(100, 80);
  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 40, 20));
  auto second_frame = generator.GetNextFrame(nullptr);

  ASSERT_FALSE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 50, 30)));

  painter.updated_region()->AddRect(DesktopRect::MakeXYWH(10, 10, 50, 30));
  second_frame = generator.GetNextFrame(nullptr);

  ASSERT_FALSE(first_frame->EqualTo(*second_frame));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(10, 10, 40, 20)));
  ASSERT_TRUE(first_frame->EqualTo(*second_frame,
                                   DesktopRect::MakeXYWH(0, 0, 50, 30)));
  ASSERT_FALSE(first_frame->EqualTo(*second_frame,
                                    DesktopRect::MakeXYWH(10, 10, 50, 30)));
}

}  // namespace webrtc
