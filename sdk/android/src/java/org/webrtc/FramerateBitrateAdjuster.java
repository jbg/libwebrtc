/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * BitrateAdjuster that adjusts the bitrate to compensate for changes in the framerate.  Used with
 * hardware codecs that assume the framerate never changes.
 */
class FramerateBitrateAdjuster extends BaseBitrateAdjuster {
  // Visible for testing.
  static final int DEFAULT_FRAMERATE_FPS = 30;

  @Override
  public void setTargets(int targetBitrateBps, int targetFramerateFps) {
    // Framerate-based bitrate adjustment always initializes to the same framerate.
    super.setTargets(targetBitrateBps, DEFAULT_FRAMERATE_FPS);

    this.targetBitrateBps = this.targetBitrateBps * DEFAULT_FRAMERATE_FPS / this.targetFramerateFps;
  }
}
