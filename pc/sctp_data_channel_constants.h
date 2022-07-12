/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_DATA_CHANNEL_CONSTANTS_H_
#define PC_SCTP_DATA_CHANNEL_CONSTANTS_H_

namespace webrtc {

// Values of priority in the DC open protocol message.
const int kDataChannelPriorityVeryLow = 128;
const int kDataChannelPriorityLow = 256;
const int kDataChannelPriorityMedium = 512;
const int kDataChannelPriorityHigh = 1024;

}  // namespace webrtc

#endif  // PC_SCTP_DATA_CHANNEL_CONSTANTS_H_
