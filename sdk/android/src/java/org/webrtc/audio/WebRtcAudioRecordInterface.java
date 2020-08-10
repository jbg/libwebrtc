/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import org.webrtc.CalledByNative;

/** Bridge for interfacing with audio_record_jni.cc */
public interface WebRtcAudioRecordInterface {
  @CalledByNative void setNativeAudioRecord(long nativeAudioRecord);

  @CalledByNative boolean isAcousticEchoCancelerSupported();

  @CalledByNative boolean isNoiseSuppressorSupported();

  // Returns true if a valid call to verifyAudioConfig() has been done. Should always be
  // checked before using the returned value of isAudioSourceMatchingRecordingSession().
  @CalledByNative boolean isAudioConfigVerified();

  // Returns true if verifyAudioConfig() succeeds. This value is set after a specific delay when
  // startRecording() has been called. Hence, should preferably be called in combination with
  // stopRecording() to ensure that it has been set properly. |isAudioConfigVerified| is
  // enabled in WebRtcAudioRecord to ensure that the returned value is valid.
  @CalledByNative boolean isAudioSourceMatchingRecordingSession();

  @CalledByNative boolean enableBuiltInAEC(boolean enable);

  @CalledByNative boolean enableBuiltInNS(boolean enable);

  @CalledByNative int initRecording(int sampleRate, int channels);

  @CalledByNative boolean startRecording();

  @CalledByNative boolean stopRecording();
}
