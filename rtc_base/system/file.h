/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_FILE_H_
#define RTC_BASE_SYSTEM_FILE_H_

#include <cstdio>
#include <memory>

#if defined(WEBRTC_WIN)
#include <Windows.h>
#include "rtc_base/string_utils.h"
#endif

// Minimal utilities to handle FILE* i/o.

namespace webrtc {

struct StdioFILEDeleter {
  void operator()(FILE* f) { std::fclose(f); }
};

enum class OpenFileMode { kReadOnly, kWriteOnly };

// Opens the file with fopen, using mode "rb" or "wb". On windows, the filename
// is converted from utf8 to utf16 and opened using _wfopen.
std::unique_ptr<FILE, StdioFILEDeleter> OpenFile(const char* file_name_utf8,
                                                 OpenFileMode mode);

}  // namespace webrtc

#endif  // RTC_BASE_SYSTEM_FILE_H_
