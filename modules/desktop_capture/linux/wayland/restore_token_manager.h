/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_

#include <map>
#include <mutex>
#include <string>

#include "modules/desktop_capture/desktop_capturer.h"

namespace webrtc {

class RestoreTokenManager {
 public:
  ~RestoreTokenManager();
  RestoreTokenManager(const RestoreTokenManager& manager) = delete;
  RestoreTokenManager& operator=(const RestoreTokenManager& manager) = delete;

  static RestoreTokenManager& GetInstance();
  static void DestroyInstance();

  void AddToken(DesktopCapturer::SourceId id, const std::string& token);
  void RemoveToken(DesktopCapturer::SourceId id);
  std::string GetToken(DesktopCapturer::SourceId id);

 private:
  RestoreTokenManager() = default;

  std::map<DesktopCapturer::SourceId, std::string> restore_tokens_;

  static RestoreTokenManager* instance_;
  static std::once_flag once_flag_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_
