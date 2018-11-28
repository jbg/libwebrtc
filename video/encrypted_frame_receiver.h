/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ENCRYPTED_FRAME_RECEIVER_H_
#define VIDEO_ENCRYPTED_FRAME_RECEIVER_H_

#include <deque>
#include <memory>

#include "api/crypto/cryptooptions.h"
#include "api/crypto/framedecryptorinterface.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// The EncryptedFrameReceiver is responsible for deciding when to pass
// decrypted received frames onto the reference finding stage. Frames can be
// delayed when frame encryption is enabled but the key hasn't arrived yet. In
// this case we stash about 1 second of encrypted frames instead of dropping
// them to prevent re-requesting the key frame. This optimization is
// particularly important on low bandwidth networks. Note stashing is only ever
// done if we have never sucessfully decrypted a frame before. After the first
// successful decryption payloads will never be stashed.
class EncryptedFrameReceiver final {
 public:
  // Constructs a new EncryptedFrameReceiver that can hold
  explicit EncryptedFrameReceiver(
      KeyFrameRequestSender* key_frame_request_sender,
      video_coding::RtpFrameReferenceFinder* reference_finder,
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
      const CryptoOptions& crypto_options);
  ~EncryptedFrameReceiver();
  // This object cannot be copied.
  EncryptedFrameReceiver(const EncryptedFrameReceiver&) = delete;
  EncryptedFrameReceiver& operator=(const EncryptedFrameReceiver&) = delete;
  // Determines whether the frame should be stashed, dropped or handed off to
  // the OnCompleteFrameCallback.
  void ManageEncryptedFrame(
      std::unique_ptr<video_coding::RtpFrameObject> encrypted_frame);

 private:
  // Represents what should be done with a given frame.
  enum class FrameDecision { kStash, kDecrypted, kDrop };

  static const size_t kMaxStashedFrames = 24;

  CryptoOptions crypto_options_;
  KeyFrameRequestSender* key_frame_request_sender_ = nullptr;
  bool first_frame_decrypted_ = false;
  bool first_frame_received_ = false;
  bool key_frame_requested_ = false;
  rtc::CriticalSection crit_;
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
  std::shared_ptr<video_coding::RtpFrameReferenceFinder> reference_finder_;
  std::deque<std::unique_ptr<video_coding::RtpFrameObject>> stashed_frames_;

  // Attempts to decrypt the frame, if it fails and no prior frames have been
  // decrypted it will return kStash. Otherwise fail to decrypts will return
  // kDrop. Successful decryptions will always return kDecrypted.
  FrameDecision DecryptFrame(video_coding::RtpFrameObject* frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Retries all the stashed frames this is triggered each time a kDecrypted
  // event occurs.
  void RetryStashedFrames() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
};

}  // namespace webrtc

#endif  // VIDEO_ENCRYPTED_FRAME_RECEIVER_H_
