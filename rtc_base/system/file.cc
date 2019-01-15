/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/system/file.h"

namespace webrtc {

std::unique_ptr<FILE, StdioFILEDeleter> OpenFile(const char* file_name_utf8,
                                                 OpenFileMode mode) {
  FILE* f;
#if defined(WEBRTC_WIN)
  std::wstring file_name = ToUtf16(file_name_utf8, strlen(file_name_utf8));
  f = _wfopen(wstr.c_str(), mode == OpenFileMode::kReadOnly ? L"rb" : L"wb");
#else
  f = std::fopen(file_name_utf8, mode == OpenFileMode::kReadOnly ? "rb" : "wb");
#endif

  return std::unique_ptr<FILE, StdioFILEDeleter>(f);
}

}  // namespace webrtc
