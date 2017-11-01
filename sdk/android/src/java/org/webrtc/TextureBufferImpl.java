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

import android.graphics.Matrix;
import java.nio.ByteBuffer;

/**
 * Android texture buffer backed by a SurfaceTextureHelper's texture. The buffer calls
 * |releaseCallback| when it is released.
 */
class TextureBufferImpl implements VideoFrame.TextureBuffer {
  private final int width;
  private final int height;
  private final Type type;
  private final int id;
  private final Matrix transformMatrix;
  private final SurfaceTextureHelper surfaceTextureHelper;
  private final Runnable releaseCallback;
  private final Object refCountLock = new Object();
  private int refCount;
  private byte[] mask;

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
      SurfaceTextureHelper surfaceTextureHelper, byte[] mask, Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.type = type;
    this.id = id;
    this.transformMatrix = transformMatrix;
    this.surfaceTextureHelper = surfaceTextureHelper;
    this.mask = mask;
    this.releaseCallback = releaseCallback;
    this.refCount = 1; // Creator implicitly holds a reference.
  }

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
      SurfaceTextureHelper surfaceTextureHelper, Runnable releaseCallback) {
    this(width, height, type, id, transformMatrix, surfaceTextureHelper, null, releaseCallback);
  }

  @Override
  public VideoFrame.TextureBuffer.Type getType() {
    return type;
  }

  @Override
  public int getTextureId() {
    return id;
  }

  @Override
  public Matrix getTransformMatrix() {
    return transformMatrix;
  }

  @Override
  public int getWidth() {
    return width;
  }

  @Override
  public int getHeight() {
    return height;
  }

  @Override
  public byte[] getMask() {
    return mask;
  }

  @Override
  public VideoFrame.I420Buffer toI420() {
    VideoFrame.I420Buffer result = surfaceTextureHelper.textureToYuv(this);

    return result;
  }

  @Override
  public void retain() {
    synchronized (refCountLock) {
      ++refCount;
    }
  }

  @Override
  public void release() {
    synchronized (refCountLock) {
      if (--refCount == 0 && releaseCallback != null) {
        releaseCallback.run();
      }
    }
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    retain();
    Matrix newMatrix = new Matrix(transformMatrix);
    newMatrix.postScale(cropWidth / (float) width, cropHeight / (float) height);
    newMatrix.postTranslate(cropX / (float) width, cropY / (float) height);
    byte[] new_mask = null;
    if (mask != null) {
      if (cropX == 0 && cropY == 0 && cropWidth == width && cropHeight == height
          && scaleWidth == width && scaleHeight == height) {
        new_mask = mask;
      } else {
        new_mask = new byte[scaleWidth * scaleHeight];
        for (int y = 0; y < scaleHeight; y++) {
          for (int x = 0; x < scaleWidth; x++) {
            int x_map = Math.round(cropX + cropWidth * (x / (float) scaleWidth));
            int y_map = Math.round(cropY + cropHeight * (y / (float) scaleHeight));

            new_mask[y * scaleWidth + x] = mask[y_map * width + x_map];
          } // end for x
        } // end for y
      } // end if
    }

    return new TextureBufferImpl(scaleWidth, scaleHeight, type, id, newMatrix, surfaceTextureHelper,
        new_mask, new Runnable() {
          @Override
          public void run() {
            release();
          }
        });
  }
}
