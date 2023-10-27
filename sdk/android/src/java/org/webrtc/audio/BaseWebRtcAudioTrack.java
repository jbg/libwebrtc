/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
package org.webrtc.audio;

import org.webrtc.CalledByNative;

public interface BaseWebRtcAudioTrack {
  @CalledByNative void setNativeAudioTrack(long nativeAudioTrack);

  @CalledByNative int initPlayout(int sampleRate, int channels, double bufferSizeFactor);

  @CalledByNative boolean startPlayout();

  @CalledByNative boolean stopPlayout();

  @CalledByNative int getStreamMaxVolume();

  @CalledByNative boolean setStreamVolume(int volume);

  @CalledByNative int getStreamVolume();

  @CalledByNative int GetPlayoutUnderrunCount();

  @CalledByNative int getBufferSizeInFrames();

  @CalledByNative int getInitialBufferSizeInFrames();

  void setSpeakerMute(boolean mute);
}
