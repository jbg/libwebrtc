//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_CODEC_H_
#define API_VOIP_VOIP_CODEC_H_

#include <map>

#include "api/audio_codecs/audio_format.h"

namespace webrtc {

class VoipCodec {
 public:
  // Set encoder type here along with its payload type to use
  virtual bool SetSendCodec(int channel,
                            int payload_type,
                            const SdpAudioFormat& encoder_spec) = 0;

  // Set decoder payload type here. In typical offer and answer model,
  // this should be called after payload type has been agreed in media
  // session.  Note that payload type can differ with same codec in each
  // direction.
  virtual bool SetReceiveCodecs(
      int channel,
      const std::map<int, SdpAudioFormat>& decoder_specs) = 0;

 protected:
  virtual ~VoipCodec() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_CODEC_H_
