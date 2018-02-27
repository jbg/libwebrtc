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

import java.util.ArrayList;
import java.util.List;
import org.webrtc.RtpParameters.Encoding;

/** Java wrapper for a C++ RtpTransceiverInterface. */
@JNINamespace("webrtc::jni")
public class RtpTransceiver {
  /** Java version of webrtc::RtpTransceiverDirection - the ordering must be kept in sync. */
  public enum RtpTransceiverDirection {
    SEND_RECV(0),
    SEND_ONLY(1),
    RECV_ONLY(2),
    INACTIVE(3);

    private final int nativeIndex;

    private RtpTransceiverDirection(int nativeIndex) {
      this.nativeIndex = nativeIndex;
    }

    @CalledByNative("RtpTransceiverDirection")
    int getNative() {
      return nativeIndex;
    }

    @CalledByNative("RtpTransceiverDirection")
    static RtpTransceiverDirection fromNativeIndex(int nativeIndex) {
      for (RtpTransceiverDirection type : RtpTransceiverDirection.values()) {
        if (type.getNative() == nativeIndex) {
          return type;
        }
      }
      throw new IllegalArgumentException(
          "Uknown native RtpTransceiverDirection type" + nativeIndex);
    }
  }

  /**
   * Tracks webrtc::RtpTransceiverInit. https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiverinit
   */
  public static class RtpTransceiverInit {
    public RtpTransceiverDirection direction = RtpTransceiverDirection.SEND_RECV;
    public List<String> streamLabels = new ArrayList<String>();
    public List<Encoding> sendEncodings = new ArrayList<Encoding>();

    @CalledByNative("RtpTransceiverInit")
    public RtpTransceiverDirection getDirection() {
      return direction;
    }

    @CalledByNative("RtpTransceiverInit")
    public List<String> getStreamLabels() {
      return streamLabels;
    }

    @CalledByNative("RtpTransceiverInit")
    public List<Encoding> getSendEncodings() {
      return sendEncodings;
    }
  }

  private final long nativeRtpTransceiver;
  private RtpSender cachedSender;
  private RtpReceiver cachedReceiver;

  @CalledByNative
  public RtpTransceiver(long nativeRtpTransceiver) {
    this.nativeRtpTransceiver = nativeRtpTransceiver;
    cachedSender = nativeGetSender(nativeRtpTransceiver);
    cachedReceiver = nativeGetReceiver(nativeRtpTransceiver);
  }

  public MediaStreamTrack.MediaType mediaType() {
    return nativeGetMediaType(nativeRtpTransceiver);
  }

  public String mid() {
    return nativeGetMid(nativeRtpTransceiver);
  }

  public RtpSender sender() {
    return cachedSender;
  }

  public RtpReceiver receiver() {
    return cachedReceiver;
  }

  public boolean stopped() {
    return nativeStopped(nativeRtpTransceiver);
  }

  public RtpTransceiverDirection direction() {
    return nativeDirection(nativeRtpTransceiver);
  }

  public RtpTransceiverDirection currentDirection() {
    return nativeCurrentDirection(nativeRtpTransceiver);
  }

  public void setDirection(RtpTransceiverDirection rtpTransceiverDirection) {
    nativeSetDirection(nativeRtpTransceiver, rtpTransceiverDirection);
  }

  public void stop() {
    nativeStop(nativeRtpTransceiver);
  }

  public void dispose() {
    cachedSender.dispose();
    cachedReceiver.dispose();
    JniCommon.nativeReleaseRef(nativeRtpTransceiver);
  }

  private static native MediaStreamTrack.MediaType nativeGetMediaType(long rtpTransceiver);

  private static native String nativeGetMid(long rtpTransceiver);

  private static native RtpSender nativeGetSender(long rtpTransceiver);

  private static native RtpReceiver nativeGetReceiver(long rtpTransceiver);

  private static native boolean nativeStopped(long rtpTransceiver);

  private static native RtpTransceiverDirection nativeDirection(long rtpTransceiver);

  private static native RtpTransceiverDirection nativeCurrentDirection(long rtpTransceiver);

  private static native void nativeStop(long rtpTransceiver);

  private static native void nativeSetDirection(
      long rtpTransceiver, RtpTransceiverDirection rtpTransceiverDirection);
}
