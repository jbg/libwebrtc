/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_RTPRECEIVER_H_
#define API_TEST_MOCK_RTPRECEIVER_H_

#include <string>
#include <vector>

#include "api/rtp_receiver_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpReceiver : public rtc::RefCountedObject<RtpReceiverInterface> {
 public:
  MockRtpReceiver() {
    ON_CALL(*this, GetFrameDecryptor).WillByDefault([this] {
      return frame_decryptor_;
    });
    ON_CALL(*this, SetFrameDecryptor)
        .WillByDefault(
            [this](rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                       frame_decryptor) {
              frame_decryptor_ = frame_decryptor;
            });
  }

  MOCK_METHOD(rtc::scoped_refptr<MediaStreamTrackInterface>,
              track,
              (),
              (const, override));
  MOCK_METHOD(std::vector<rtc::scoped_refptr<MediaStreamInterface>>,
              streams,
              (),
              (const, override));
  MOCK_METHOD(cricket::MediaType, media_type, (), (const, override));
  MOCK_METHOD(std::string, id, (), (const, override));
  MOCK_METHOD(RtpParameters, GetParameters, (), (const, override));
  MOCK_METHOD(bool,
              SetParameters,
              (const webrtc::RtpParameters& parameters),
              (override));
  MOCK_METHOD(void, SetObserver, (RtpReceiverObserverInterface*), (override));
  MOCK_METHOD(void,
              SetJitterBufferMinimumDelay,
              (absl::optional<double>),
              (override));
  MOCK_METHOD(std::vector<RtpSource>, GetSources, (), (const, override));
  MOCK_METHOD(void,
              SetFrameDecryptor,
              (rtc::scoped_refptr<webrtc::FrameDecryptorInterface>),
              (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>,
              GetFrameDecryptor,
              (),
              (const, override));

 private:
  rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor_ =
      nullptr;
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_RTPRECEIVER_H_
