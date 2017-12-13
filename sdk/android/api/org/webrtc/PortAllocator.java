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

/** Java wrapper for a C++ PortAllocator. */
public class PortAllocator {
  protected long nativePortAllocator;

  public PortAllocator() {
    this.nativePortAllocator = 0;
  }

  public PortAllocator(long nativePortAllocator) {
    this.nativePortAllocator = nativePortAllocator;
  }

  public long release() {
    long nativePointer = nativePortAllocator;
    this.nativePortAllocator = 0;
    return nativePointer;
  }

  public void dispose() {
    if (nativePortAllocator != 0) {
      nativeFreePortAllocator(nativePortAllocator);
      nativePortAllocator = 0;
    }
  }

  private static native void nativeFreePortAllocator(long nativePortAllocator);
}
