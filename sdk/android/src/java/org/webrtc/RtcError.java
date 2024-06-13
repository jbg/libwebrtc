/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * RtcError class is used to handle errors obtainable from WebRTC operations.
 */
public class RtcError {
    private final RtcException error;

    @CalledByNative
    RtcError(RtcException error) {
        this.error = error;
    }

    public boolean isSuccess() {
        return error == null;
    }

    public boolean isError() {
        return error != null;
    }

    /** Retrieve the error from the WebRTC operation. */
    public RtcException error() {
        return error;
    }

    /** Throws the error if it is not null. */
    public void throwError() {
        if (error != null) {
            throw error;
        }
    }
}
