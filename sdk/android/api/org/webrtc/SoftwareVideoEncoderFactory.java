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

import android.support.annotation.Nullable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class SoftwareVideoEncoderFactory implements VideoEncoderFactory {
  private static final String TAG = "SoftwareVideoEncoderFactory";

  @Nullable
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo codecInfo) {
    VideoCodecMimeType codecType = null;
    try {
      codecType = VideoCodecMimeType.fromSdpCodecName(codecInfo.getName());
    } catch (IllegalArgumentException e) {
      Logging.e(TAG, "Unknown encoder: " + codecInfo.getName(), e);
      return null;
    }

    if (codecType == VideoCodecMimeType.VP8) {
      return new LibvpxVp8Encoder();
    }
    if (codecType == VideoCodecMimeType.VP9 && LibvpxVp9Encoder.nativeIsSupported()) {
      return new LibvpxVp9Encoder();
    }
    if (codecType == VideoCodecMimeType.AV1 && LibaomAv1Encoder.nativeIsSupported()) {
      return new LibaomAv1Encoder();
    }

    Logging.w(TAG, "Unsupported encoder: " + codecInfo.getName());
    return null;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return supportedCodecs();
  }

  static VideoCodecInfo[] supportedCodecs() {
    List<VideoCodecInfo> codecs = new ArrayList<VideoCodecInfo>();

    codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP8.toSdpCodecName(), new HashMap<>()));
    if (LibvpxVp9Encoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP9.toSdpCodecName(), new HashMap<>()));
    }
    if (LibaomAv1Encoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.AV1.toSdpCodecName(), new HashMap<>()));
    }

    return codecs.toArray(new VideoCodecInfo[codecs.size()]);
  }
}
