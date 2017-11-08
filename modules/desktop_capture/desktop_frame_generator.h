/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_GENERATOR_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_GENERATOR_H_

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/rgba_color.h"
#include "modules/desktop_capture/shared_memory.h"

namespace webrtc {

// An interface to generate a DesktopFrame.
class DesktopFrameGenerator {
 public:
  DesktopFrameGenerator();
  virtual ~DesktopFrameGenerator();

  virtual std::unique_ptr<DesktopFrame> GetNextFrame(
      SharedMemoryFactory* factory) = 0;
};

// An interface to paint a DesktopFrame. This interface is used by
// PainterDesktopFrameGenerator.
class DesktopFramePainter {
 public:
  DesktopFramePainter();
  virtual ~DesktopFramePainter();

  virtual bool Paint(DesktopFrame* frame, DesktopRegion* updated_region) = 0;
};

// An implementation of DesktopFrameGenerator to take care about the
// DesktopFrame size, filling updated_region(), etc, but leaves the real
// painting work to a DesktopFramePainter implementation.
class PainterDesktopFrameGenerator final : public DesktopFrameGenerator {
 public:
  PainterDesktopFrameGenerator();
  ~PainterDesktopFrameGenerator() override;

  std::unique_ptr<DesktopFrame> GetNextFrame(
      SharedMemoryFactory* factory) override;

  // Sets the size of the frame which will be returned in next GetNextFrame()
  // call.
  DesktopSize* size();

  // Decides whether PainterDesktopFrameGenerator returns a frame in next
  // Capture() callback. If |return_frame_| is true,
  // PainterDesktopFrameGenerator will create a frame according to both |size_|
  // and SharedMemoryFactory input, and uses Paint() function to paint it.
  void set_return_frame(bool return_frame);

  // Decides whether PainterDesktopFrameGenerator returns a frame with updated
  // regions. PainterDesktopFrameGenerator will keep
  // DesktopFrame::updated_region() empty if this field is false.
  void set_provide_updated_region_hints(bool provide_updated_region_hints);

  // Decides whether PainterDesktopFrameGenerator randomly enlarges updated
  // regions in the DesktopFrame. Set this field to true to simulate an
  // inaccurate return of updated regions from OS APIs.
  void set_enlarge_updated_region(bool enlarge_updated_region);

  // The range to enlarge a updated region if |enlarge_updated_region_| is true.
  // If this field is less than zero, it will be treated as zero, and
  // |enlarge_updated_region_| will be ignored.
  void set_enlarge_range(int enlarge_range);

  // Decides whether PainterDesktopFrameGenerator randomly add some updated
  // regions in the DesktopFrame. Set this field to true to simulate an
  // inaccurate return of updated regions from OS APIs.
  void set_add_random_updated_region(bool add_random_updated_region);

  // Sets the painter object to do the real painting work, if no |painter_| has
  // been set to this instance, the DesktopFrame returned by GetNextFrame()
  // function will keep in an undefined but valid state.
  // PainterDesktopFrameGenerator does not take ownership of the |painter|.
  void set_desktop_frame_painter(DesktopFramePainter* painter);

 private:
  DesktopSize size_;
  bool return_frame_;
  bool provide_updated_region_hints_;
  bool enlarge_updated_region_;
  int enlarge_range_;
  bool add_random_updated_region_;
  DesktopFramePainter* painter_;
};

// An implementation of DesktopFramePainter to paint predefined colors on each
// rectangle of updated_region(), and white elsewhere.
class ColorfulDesktopFramePainter final : public DesktopFramePainter {
 public:
  ColorfulDesktopFramePainter();
  ~ColorfulDesktopFramePainter() override;

  // The colorful regions of the frame which will be returned in next Paint()
  // call. ColorfulDesktopFramePainter will draw a white frame with predefined
  // colors in each rectangle in the |updated_region_|. Each Paint() call
  // consumes |updated_region_|.
  DesktopRegion* updated_region();

  // Allows consumers to modify the colors used by this painter.
  std::vector<RgbaColor>* colors();

  // DesktopFramePainter interface.
  bool Paint(DesktopFrame* frame, DesktopRegion* updated_region) override;

 private:
  // If |colors_| is empty, painter uses black.
  std::vector<RgbaColor> colors_;
  DesktopRegion updated_region_;
};

// An implementation of DesktopFramePainter to paint black on
// mutable_updated_region(), and white elsewhere.
class BlackWhiteDesktopFramePainter final : public DesktopFramePainter {
 public:
  BlackWhiteDesktopFramePainter();
  ~BlackWhiteDesktopFramePainter() override;

  // The black regions of the frame which will be returned in next Paint()
  // call. BlackWhiteDesktopFramePainter will draw a white frame with black in
  // the |updated_region_|. Each Paint() call will consume |updated_region_|.
  DesktopRegion* updated_region();

  // DesktopFramePainter interface.
  bool Paint(DesktopFrame* frame, DesktopRegion* updated_region) override;

 private:
  ColorfulDesktopFramePainter painter_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_GENERATOR_H_
