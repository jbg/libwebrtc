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
 * RTCErrorOr class is used to handle errors obtainable from WebRTC operations. It may either
 * contain a value of type T (indicating success), or an RTCException (indicating failure).
 *
 * @param <T> - The type of the value wrapped by this class.
 */
public class RTCErrorOr<T> {
    private final T value;
    private final RTCException error;

    private RTCErrorOr(T value, RTCException error) {
        this.value = value;
        this.error = error;
    }

    /** Constructor initializing the value field. This implies a successful WebRTC operation. */
    @CalledByNative
    public static <T> RTCErrorOr<T> success(@NonNull T value) {
        return new RTCErrorOr<>(value, null);
    }

    /** Constructor initializing the error field. This implies a failed WebRTC operation. */
    @CalledByNative
    public static <T> RTCErrorOr<T> error(@NonNull String error) {
        return new RTCErrorOr<>(null, new RTCException(error));
    }

    public boolean isError() {
        return error != null;
    }

    /**
     * Retrieve the value obtained from the WebRTC operation. This function should be called only if
     * isError is false.
     *
     * @throws RTCException - if there was an error in the operation.
     */
    public T value() {
        if (error != null) {
            throw error;
        }
        return value;
    }

    @Nullable
    public T valueOrNull() {
        return value;
    }

    /** Retrieve the error from the WebRTC operation. */
    public RTCException error() {
        return error;
    }
}
