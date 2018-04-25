/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.Looper;

/** Helper class for task_queue_alooper.cc. */
class LooperTaskQueueHelper {
  private final long wakeupPtr;
  private final Runnable wakeupRunnable = this ::onWakeupDelayed;

  private Handler handler;

  @CalledByNative
  public LooperTaskQueueHelper(long wakeupPtr) {
    this.wakeupPtr = wakeupPtr;
  }

  /**
   * Prepares the helper. Must be called on the task queue. Must be called before any other method
   * calls.
   */
  @CalledByNative
  public void prepare() {
    Looper.prepare();
    handler = new Handler();
  }

  /**
   * Schedules the next wake up in {@code delayMs} milliseconds. {@link #nativeOnWakeupDelayed()}
   * will be next called after this delay.
   */
  @CalledByNative
  public void scheduleWakeup(long delayMs) {
    handler.removeCallbacks(wakeupRunnable);
    handler.postDelayed(wakeupRunnable, delayMs);
  }

  /** Runs the looper loop until quit is called. Must be called on the task queue. */
  @CalledByNative
  public static void loop() {
    Looper.loop();
  }

  /** Stops the looper loop. Must be called on the task queue. */
  @CalledByNative
  public static void quit() {
    Looper.myLooper().quit();
  }

  private void onWakeupDelayed() {
    nativeOnWakeupDelayed(wakeupPtr);
  }

  private static native void nativeOnWakeupDelayed(long wakeupPtr);
}
