/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encrypted_frame_receiver.h"

#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/system/fallthrough.h"

namespace webrtc {

EncryptedFrameReceiver::EncryptedFrameReceiver(
    KeyFrameRequestSender* key_frame_request_sender,
    video_coding::RtpFrameReferenceFinder* reference_finder,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const CryptoOptions& crypto_options)
    : crypto_options_(crypto_options),
      key_frame_request_sender_(key_frame_request_sender),
      frame_decryptor_(std::move(frame_decryptor)),
      reference_finder_(reference_finder) {}

EncryptedFrameReceiver::~EncryptedFrameReceiver() {}

void EncryptedFrameReceiver::ManageEncryptedFrame(
    std::unique_ptr<video_coding::RtpFrameObject> encrypted_frame) {
  rtc::CritScope lock(&crit_);

  // Immediately request a key frame if the stream doesn't start with one.
  if (!first_frame_received_) {
    first_frame_received_ = true;
    if (encrypted_frame->FrameType() != kVideoFrameKey) {
      key_frame_requested_ = true;
      key_frame_request_sender_->RequestKeyFrame();
    }
  }

  switch (DecryptFrame(encrypted_frame.get())) {
    case FrameDecision::kStash:
      if (stashed_frames_.size() > kMaxStashedFrames) {
        stashed_frames_.pop_front();
      }
      stashed_frames_.push_back(std::move(encrypted_frame));
      break;
    case FrameDecision::kDecrypted:
      RetryStashedFrames();
      reference_finder_->ManageFrame(std::move(encrypted_frame));
      break;
    case FrameDecision::kDrop:
      break;
  }
}

EncryptedFrameReceiver::FrameDecision EncryptedFrameReceiver::DecryptFrame(
    video_coding::RtpFrameObject* frame) {
  // Optionally attempt to decrypt the raw video frame if it was provided.
  if (frame_decryptor_ == nullptr) {
    RTC_LOG(LS_WARNING) << "Frame decryption required but not attached to this "
                           "stream. Dropping frame.";
    return FrameDecision::kDrop;
  }
  // When using encryption we expect the frame to have the generic descriptor.
  absl::optional<RtpGenericFrameDescriptor> descriptor =
      frame->GetGenericFrameDescriptor();
  if (!descriptor) {
    RTC_LOG(LS_ERROR) << "No generic frame descriptor found dropping frame.";
    return FrameDecision::kDrop;
  }
  // Retrieve the bitstream of the encrypted video frame.
  rtc::ArrayView<const uint8_t> encrypted_frame_bitstream(frame->Buffer(),
                                                          frame->size());
  // Retrieve the maximum possible size of the decrypted payload.
  const size_t max_plaintext_byte_size =
      frame_decryptor_->GetMaxPlaintextByteSize(cricket::MEDIA_TYPE_VIDEO,
                                                frame->size());
  RTC_CHECK(max_plaintext_byte_size <= frame->size());
  // Place the decrypted frame inline into the existing frame.
  rtc::ArrayView<uint8_t> inline_decrypted_bitstream(frame->MutableBuffer(),
                                                     max_plaintext_byte_size);
  // Attempt to decrypt the video frame.
  size_t bytes_written = 0;
  if (frame_decryptor_->Decrypt(
          cricket::MEDIA_TYPE_VIDEO, /*csrcs=*/{},
          /*additional_data=*/nullptr, encrypted_frame_bitstream,
          inline_decrypted_bitstream, &bytes_written) != 0) {
    // Only stash frames if we have never decrypted a frame before.
    return first_frame_decrypted_ ? FrameDecision::kDrop
                                  : FrameDecision::kStash;
  }
  RTC_CHECK(bytes_written <= max_plaintext_byte_size);
  // Update the frame to contain just the written bytes.
  frame->SetLength(bytes_written);

  if (!first_frame_decrypted_) {
    first_frame_decrypted_ = true;
    if (!key_frame_requested_ && frame->FrameType() != kVideoFrameKey) {
      key_frame_requested_ = true;
      key_frame_request_sender_->RequestKeyFrame();
    }
  }
  return FrameDecision::kDecrypted;
}

void EncryptedFrameReceiver::RetryStashedFrames() {
  for (auto frame_it = stashed_frames_.begin();
       frame_it != stashed_frames_.end();) {
    switch (DecryptFrame(frame_it->get())) {
      case FrameDecision::kStash:
        ++frame_it;
        break;
      case FrameDecision::kDecrypted:
        reference_finder_->ManageFrame(std::move(*frame_it));
        RTC_FALLTHROUGH();
      case FrameDecision::kDrop:
        frame_it = stashed_frames_.erase(frame_it);
        break;
    }
  }
}

}  // namespace webrtc
