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
import android.media.AudioAttributes;
import android.media.AudioManager;
import androidx.annotation.Nullable;
import org.webrtc.audio.BaseWebRtcAudioTrack;

public interface AudioTrackCreator {
  BaseWebRtcAudioTrack createAudioTrack(Context context, AudioManager audioManager,
      @Nullable AudioAttributes audioAttributes,
      @Nullable JavaAudioDeviceModule.AudioTrackErrorCallback errorCallback,
      @Nullable JavaAudioDeviceModule.AudioTrackStateCallback stateCallback, boolean useLowLatency,
      boolean enableVolumeLogger);
}
