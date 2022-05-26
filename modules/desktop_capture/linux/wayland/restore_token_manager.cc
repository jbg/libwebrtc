/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"

namespace webrtc {

RestoreTokenManager* RestoreTokenManager::instance_ = nullptr;
std::once_flag RestoreTokenManager::once_flag_;

RestoreTokenManager::~RestoreTokenManager() {
  DestroyInstance();
}

// static
RestoreTokenManager& RestoreTokenManager::GetInstance() {
  std::call_once(once_flag_, [] { instance_ = new RestoreTokenManager(); });
  return *instance_;
}

// static
void RestoreTokenManager::DestroyInstance() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

void RestoreTokenManager::AddToken(DesktopCapturer::SourceId id,
                                   const std::string& token) {
  restore_tokens_.insert({id, token});
}

void RestoreTokenManager::RemoveToken(DesktopCapturer::SourceId id) {
  restore_tokens_.erase(id);
}

std::string RestoreTokenManager::GetToken(DesktopCapturer::SourceId id) {
  return restore_tokens_[id];
}

}  // namespace webrtc
