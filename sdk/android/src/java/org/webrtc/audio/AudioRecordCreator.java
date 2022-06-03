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

import android.content.Context;
import android.media.AudioManager;
import androidx.annotation.Nullable;
import java.util.concurrent.ScheduledExecutorService;
import org.webrtc.audio.BaseWebRtcAudioRecord;

public interface AudioRecordCreator {
  BaseWebRtcAudioRecord createAudioRecord(Context context, ScheduledExecutorService scheduler,
      AudioManager audioManager, int audioSource, int audioFormat,
      @Nullable JavaAudioDeviceModule.AudioRecordErrorCallback errorCallback,
      @Nullable JavaAudioDeviceModule.AudioRecordStateCallback stateCallback,
      @Nullable JavaAudioDeviceModule.SamplesReadyCallback audioSamplesReadyCallback,
      boolean isAcousticEchoCancelerSupported, boolean isNoiseSuppressorSupported);
}
