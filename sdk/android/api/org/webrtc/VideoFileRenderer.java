/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.HandlerThread;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Can be used to save the video frames to file.
 */
@JNINamespace("webrtc::jni")
public class VideoFileRenderer implements VideoRenderer.Callbacks, VideoSink {
  private static final String TAG = "VideoFileRenderer";

  private final HandlerThread renderThread;
  private final Handler renderThreadHandler;
  private final BlockingQueue<VideoFrame> queuedFrames = new LinkedBlockingQueue<>();
  private final FileOutputStream videoOutFile;
  private final String outputFileName;
  private final int outputFileWidth;
  private final int outputFileHeight;
  private final int outputFrameSize;
  private final ByteBuffer outputFrameBuffer;
  private EglBase eglBase;
  private YuvConverter yuvConverter;
  private ArrayList<ByteBuffer> rawFrames = new ArrayList<>();

  public VideoFileRenderer(String outputFile, int outputFileWidth, int outputFileHeight,
      final EglBase.Context sharedContext) throws IOException {
    if ((outputFileWidth % 2) == 1 || (outputFileHeight % 2) == 1) {
      throw new IllegalArgumentException("Does not support uneven width or height");
    }

    this.outputFileName = outputFile;
    this.outputFileWidth = outputFileWidth;
    this.outputFileHeight = outputFileHeight;

    outputFrameSize = outputFileWidth * outputFileHeight * 3 / 2;
    outputFrameBuffer = ByteBuffer.allocateDirect(outputFrameSize);

    videoOutFile = new FileOutputStream(outputFile);
    videoOutFile.write(
        ("YUV4MPEG2 C420 W" + outputFileWidth + " H" + outputFileHeight + " Ip F30:1 A1:1\n")
            .getBytes(Charset.forName("US-ASCII")));

    renderThread = new HandlerThread(TAG);
    renderThread.start();
    renderThreadHandler = new Handler(renderThread.getLooper());

    ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, new Runnable() {
      @Override
      public void run() {
        eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_BUFFER);
        eglBase.createDummyPbufferSurface();
        eglBase.makeCurrent();
        yuvConverter = new YuvConverter();
      }
    });
  }

  @Override
  public void renderFrame(final VideoRenderer.I420Frame i420Frame) {
    final VideoFrame frame = i420Frame.toVideoFrame();
    onFrame(frame);
    frame.release();
  }

  @Override
  public void onFrame(VideoFrame frame) {
    frame.retain();
    renderThreadHandler.post(() -> renderFrameOnRenderThread(frame));
  }

  private void renderFrameOnRenderThread(VideoFrame frame) {
    final VideoFrame.Buffer buffer = frame.getBuffer();

    final float frameAspectRatio =
        (float) frame.getRotatedWidth() / (float) frame.getRotatedHeight();
    final float fileAspectRation = (float) outputFileWidth / outputFileHeight;

    // Calculate cropping to equalize the aspect ratio.
    int cropWidth = frame.getRotatedWidth();
    int cropHeight = frame.getRotatedHeight();
    if (fileAspectRation > frameAspectRatio) {
      cropHeight *= frameAspectRatio / fileAspectRation;
    } else {
      cropWidth *= fileAspectRation / frameAspectRatio;
    }

    if (frame.getRotation() % 180 != 0) {
      int tmp = cropWidth;
      cropWidth = cropHeight;
      cropHeight = tmp;
    }

    int cropX = (buffer.getWidth() - cropWidth) / 2;
    int cropY = (buffer.getHeight() - cropHeight) / 2;

    final int scaledWidth = frame.getRotation() % 180 == 0 ? outputFileWidth : outputFileHeight;
    final int scaledHeight = frame.getRotation() % 180 == 0 ? outputFileHeight : outputFileWidth;

    final VideoFrame.Buffer scaledBuffer =
        buffer.cropAndScale(cropX, cropY, cropWidth, cropHeight, scaledWidth, scaledHeight);
    frame.release();

    final VideoFrame.I420Buffer i420 = scaledBuffer.toI420();
    scaledBuffer.release();

    ByteBuffer byteBuffer = JniCommon.nativeAllocateByteBuffer(outputFrameSize);
    YuvHelper.I420Rotate(i420.getDataY(), i420.getStrideY(), i420.getDataU(), i420.getStrideU(),
        i420.getDataV(), i420.getStrideV(), byteBuffer, i420.getWidth(), i420.getHeight(),
        frame.getRotation());
    i420.release();

    byteBuffer.rewind();
    rawFrames.add(byteBuffer);
  }

  /**
   * Release all resources. All already posted frames will be rendered first.
   */
  public void release() {
    final CountDownLatch cleanupBarrier = new CountDownLatch(1);
    renderThreadHandler.post(() -> {
      yuvConverter.release();
      eglBase.release();
      renderThread.quit();
      cleanupBarrier.countDown();
    });
    ThreadUtils.awaitUninterruptibly(cleanupBarrier);
    try {
      for (ByteBuffer buffer : rawFrames) {
        videoOutFile.write("FRAME\n".getBytes(Charset.forName("US-ASCII")));

        byte[] data = new byte[outputFrameSize];
        buffer.get(data);

        videoOutFile.write(data);

        JniCommon.nativeFreeByteBuffer(buffer);
      }
      videoOutFile.close();
      Logging.d(TAG,
          "Video written to disk as " + outputFileName + ". Number frames are " + rawFrames.size()
              + " and the dimension of the frames are " + outputFileWidth + "x" + outputFileHeight
              + ".");
    } catch (IOException e) {
      Logging.e(TAG, "Error writing video to disk", e);
    }
  }
}
