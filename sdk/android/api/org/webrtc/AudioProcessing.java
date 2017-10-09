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

/** Java wrapper for an AudioProcessing module. */
public class AudioProcessing {
  // Pointer to a rtc::scoped_refptr<AudioProcessing>.
  // Package protected for PeerConnectionFactory.
  final long nativePointer;

  // Takes an injectable post processing module.
  public AudioProcessing(PostProcessing postProcessor) {
    nativePointer = nativeCreateAudioProcessing(postProcessor.getNativePointer());
  }

  // Must be called if and only if the AudioProcessing instance is not used to create a
  // PeerConnectionFactory, to avoid memory leaks.
  public void release() {
    JniCommon.nativeReleaseRef(nativePointer);
  }

  private static native long nativeCreateAudioProcessing(long postProcessor);
}
