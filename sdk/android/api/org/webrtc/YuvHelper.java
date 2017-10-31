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

import java.nio.ByteBuffer;

/**
 * Wraps libyuv methods to Java.
 */
public class YuvHelper {
  public static native void I420Copy(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU,
      int srcStrideU, ByteBuffer srcV, int srcStrideV, ByteBuffer dstY, int dstStrideY,
      ByteBuffer dstU, int dstStrideU, ByteBuffer dstV, int dstStrideV, int width, int height);
  public static native void I420ToNV12(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU,
      int srcStrideU, ByteBuffer srcV, int srcStrideV, ByteBuffer dstY, int dstStrideY,
      ByteBuffer dstUV, int dstStrideUv, int width, int height);
}
