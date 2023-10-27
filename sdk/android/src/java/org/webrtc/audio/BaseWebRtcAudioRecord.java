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
import java.nio.ByteBuffer;
import org.webrtc.CalledByNative;

public abstract class BaseWebRtcAudioRecord {
  @CalledByNative public abstract void setNativeAudioRecord(long nativeAudioRecord);

  @CalledByNative public abstract boolean isAcousticEchoCancelerSupported();

  @CalledByNative public abstract boolean isNoiseSuppressorSupported();

  @CalledByNative public abstract boolean isAudioConfigVerified();

  @CalledByNative public abstract boolean isAudioSourceMatchingRecordingSession();

  @CalledByNative public abstract boolean enableBuiltInAEC(boolean enable);

  @CalledByNative public abstract boolean enableBuiltInNS(boolean enable);

  @CalledByNative public abstract int initRecording(int sampleRate, int channels);

  @CalledByNative public abstract boolean startRecording();

  @CalledByNative public abstract boolean stopRecording();

  public abstract void setMicrophoneMute(boolean mute);

  public abstract boolean setNoiseSuppressorEnabled(boolean enabled);

  public abstract void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice);

  // Rather than passing the ByteBuffer with every callback (requiring
  // the potentially expensive GetDirectBufferAddress) we simply have the
  // the native class cache the address to the memory once.
  public native void nativeCacheDirectBufferAddress(
      long nativeAudioRecordJni, ByteBuffer byteBuffer);
  public native void nativeDataIsRecorded(
      long nativeAudioRecordJni, int bytes, long captureTimestampNs);
}