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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import java.nio.ByteBuffer;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class YuvHelperTest {
  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader());
  }

  @SmallTest
  @Test
  public void testI420Copy() {
    final int width = 3;
    final int height = 3;
    final int chromaWidth = 2;
    final int chromaHeight = 2;

    final int testStrideY = 3;
    final int testStrideU = 2;
    final int testStrideV = 4;

    final ByteBuffer testY = ByteBuffer.allocateDirect(height * testStrideY);
    final ByteBuffer testU = ByteBuffer.allocateDirect(chromaHeight * testStrideU);
    final ByteBuffer testV = ByteBuffer.allocateDirect(chromaHeight * testStrideV);
    testY.put(new byte[] {1, 2, 3, 4, 5, 6});
    testU.put(new byte[] {51, 52, 53, 54});
    testV.put(new byte[] {101, 102, 103, 104, 105, 106, 107, 108});

    final int dstStrideY = width;
    final int dstStrideU = chromaWidth;
    final int dstStrideV = chromaWidth;

    final ByteBuffer dstY = ByteBuffer.allocateDirect(height * dstStrideY);
    final ByteBuffer dstU = ByteBuffer.allocateDirect(chromaHeight * dstStrideU);
    final ByteBuffer dstV = ByteBuffer.allocateDirect(chromaHeight * dstStrideV);

    YuvHelper.I420Copy(testY, testStrideY, testU, testStrideU, testV, testStrideV, dstY, dstStrideY,
        dstU, dstStrideU, dstV, dstStrideV, width, height);

    assertByteBufferContentEquals(new byte[] {1, 2, 3, 4, 5, 6}, dstY);
    assertByteBufferContentEquals(new byte[] {51, 52, 53, 54}, dstU);
    assertByteBufferContentEquals(new byte[] {101, 102, 105, 106}, dstV);
  }

  @SmallTest
  @Test
  public void testI420ToNV12() {
    final int width = 3;
    final int height = 3;
    final int chromaWidth = 2;
    final int chromaHeight = 2;

    final int testStrideY = 3;
    final int testStrideU = 2;
    final int testStrideV = 4;

    final ByteBuffer testY = ByteBuffer.allocateDirect(height * testStrideY);
    final ByteBuffer testU = ByteBuffer.allocateDirect(chromaHeight * testStrideU);
    final ByteBuffer testV = ByteBuffer.allocateDirect(chromaHeight * testStrideV);
    testY.put(new byte[] {1, 2, 3, 4, 5, 6});
    testU.put(new byte[] {51, 52, 53, 54});
    testV.put(new byte[] {101, 102, 103, 104, 105, 106, 107, 108});

    final int dstStrideY = width;
    final int dstStrideUV = chromaWidth * 2;

    final ByteBuffer dstY = ByteBuffer.allocateDirect(height * dstStrideY);
    final ByteBuffer dstUV = ByteBuffer.allocateDirect(2 * chromaHeight * dstStrideUV);

    YuvHelper.I420ToNV12(testY, testStrideY, testU, testStrideU, testV, testStrideV, dstY,
        dstStrideY, dstUV, dstStrideUV, width, height);

    assertByteBufferContentEquals(new byte[] {1, 2, 3, 4, 5, 6}, dstY);
    assertByteBufferContentEquals(new byte[] {51, 101, 52, 102, 53, 105, 54, 106}, dstUV);
  }

  private static void assertByteBufferContentEquals(byte[] expected, ByteBuffer test) {
    assertTrue(
        "ByteBuffer is too small. Expected " + expected.length + " but was " + test.capacity(),
        test.capacity() >= expected.length);
    for (int i = 0; i < expected.length; i++) {
      assertEquals("Unexpected ByteBuffer contents at index: " + i, expected[i], test.get(i));
    }
  }
}
