/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CALL_MEDIA_TYPE_H_
#define API_CALL_MEDIA_TYPE_H_

namespace webrtc {

enum class MediaType {
  ANY,
  AUDIO,
  VIDEO,
  DATA
};

}  // namespace webrtc

#endif  // API_CALL_MEDIA_TYPE_H_
