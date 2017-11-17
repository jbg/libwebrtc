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

import org.w3c.dom.Text;

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
  private int mask_texture_id;
  private int mask_width;
  private int mask_height;
  private Matrix mask_transform;

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
      SurfaceTextureHelper surfaceTextureHelper, Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.type = type;
    this.id = id;
    this.transformMatrix = transformMatrix;
    this.surfaceTextureHelper = surfaceTextureHelper;
    this.releaseCallback = releaseCallback;
    this.refCount = 1; // Creator implicitly holds a reference.
    this.mask = null;
    this.mask_texture_id = -1;
    this.mask_width = 0;
    this.mask_height = 0;
  }

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
                           SurfaceTextureHelper surfaceTextureHelper,
                           int mask_texture_id, int mask_width, int mask_height, Matrix maskTransform,
                           Runnable releaseCallback) {
    this(width, height, type, id, transformMatrix, surfaceTextureHelper, releaseCallback);
    this.mask_texture_id = mask_texture_id;
    this.mask_width = mask_width;
    this.mask_height = mask_height;
    this.mask_transform = maskTransform;
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
  public int getMaskTextureId() {return mask_texture_id;}

  @Override
  public Matrix getMaskTransform() {return mask_transform;}

  @Override
  public int getMaskWidth() {return mask_width;}

  @Override
  public int getMaskHeight() {return mask_height;}

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

    Matrix newMaskTransform = new Matrix(mask_transform);
    newMaskTransform.postScale(cropWidth / (float) width, cropHeight / (float) height);
    newMaskTransform.postTranslate(cropX / (float) width, cropY / (float) height);


    return new TextureBufferImpl(scaleWidth, scaleHeight, type, id, newMatrix, surfaceTextureHelper,
            mask_texture_id, mask_width, mask_height, newMaskTransform, new Runnable() {
      @Override
      public void run() {
        release();
      }
    });

  }

  @Override
  public VideoFrame.Buffer spawnMask() {
    if (mask_texture_id == -1) return null;

    retain();
    int mask_id = mask_texture_id;
    mask_texture_id = -1;
    return new TextureBufferImpl(width, height, Type.RGB, mask_id, mask_transform, surfaceTextureHelper,
            -1, 0, 0, mask_transform, new Runnable() {
      @Override
      public void run() {
        release();
      }
    });
  }
}
