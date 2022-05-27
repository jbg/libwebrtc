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

import android.media.AudioDeviceInfo;
import androidx.annotation.Nullable;
import org.webrtc.CalledByNative;

public interface BaseWebRtcAudioRecord {
  @CalledByNative void setNativeAudioRecord(long nativeAudioRecord);

  @CalledByNative boolean isAcousticEchoCancelerSupported();

  @CalledByNative boolean isNoiseSuppressorSupported();

  @CalledByNative boolean isAudioConfigVerified();

  @CalledByNative boolean isAudioSourceMatchingRecordingSession();

  @CalledByNative boolean enableBuiltInAEC(boolean enable);

  @CalledByNative boolean enableBuiltInNS(boolean enable);

  @CalledByNative int initRecording(int sampleRate, int channels);

  @CalledByNative boolean startRecording();

  @CalledByNative boolean stopRecording();

  void setMicrophoneMute(boolean mute);

  void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice);
}