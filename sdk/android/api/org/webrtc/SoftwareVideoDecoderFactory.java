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

public class SoftwareVideoDecoderFactory implements VideoDecoderFactory {
  private static final String TAG = "SoftwareVideoDecoderFactory";

  @Deprecated
  @Nullable
  @Override
  public VideoDecoder createDecoder(String codecType) {
    return createDecoder(new VideoCodecInfo(codecType, new HashMap<>()));
  }

  @Nullable
  @Override
  public VideoDecoder createDecoder(VideoCodecInfo codecInfo) {
    VideoCodecMimeType codecType = null;
    try {
      codecType = VideoCodecMimeType.fromSdpCodecName(codecInfo.getName());
    } catch (IllegalArgumentException e) {
      Logging.d(TAG, "Unrecognized decoder " + codecInfo.getName());
      return null;
    }

    if (codecType == VideoCodecMimeType.VP8) {
      return new LibvpxVp8Decoder();
    }
    if (codecType == VideoCodecMimeType.VP9 && LibvpxVp9Decoder.nativeIsSupported()) {
      return new LibvpxVp9Decoder();
    }
    if (codecType == VideoCodecMimeType.AV1 && LibaomAv1Decoder.nativeIsSupported()) {
      return new LibaomAv1Decoder();
    }

    return null;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return supportedCodecs();
  }

  static VideoCodecInfo[] supportedCodecs() {
    List<VideoCodecInfo> codecs = new ArrayList<VideoCodecInfo>();

    codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP8.toSdpCodecName(), new HashMap<>()));
    if (LibvpxVp9Decoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP9.toSdpCodecName(), new HashMap<>()));
    }
    if (LibaomAv1Decoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.AV1.toSdpCodecName(), new HashMap<>()));
    }

    return codecs.toArray(new VideoCodecInfo[codecs.size()]);
  }
}
