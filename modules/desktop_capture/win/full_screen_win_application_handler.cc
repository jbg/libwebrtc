/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/full_screen_win_application_handler.h"
#include <shellapi.h>
#include <winternl.h>
#include <algorithm>
#include <memory>
#include <string>
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"  // For RTC_LOG_GLE

namespace webrtc {
namespace {

// counterpart for string_trim
std::wstring wstring_trim(const std::wstring& s) {
  static const wchar_t kWhitespace[] = L" \n\r\t";
  const std::wstring::size_type first = s.find_first_not_of(kWhitespace);
  const std::wstring::size_type last = s.find_last_not_of(kWhitespace);
  if (first == std::wstring::npos || last == std::wstring::npos) {
    return L"";
  }
  return s.substr(first, last - first + 1);
}

std::wstring WindowText(HWND window) {
  constexpr size_t kBufferLength = 256;
  WCHAR buffer[kBufferLength];
  const int len = ::GetWindowTextW(window, buffer, kBufferLength);
  return len > 0 ? std::wstring{buffer, len} : L"";
}

DWORD WindowProcess(HWND window) {
  DWORD dwProcessId = 0;
  ::GetWindowThreadProcessId(window, &dwProcessId);
  return dwProcessId;
}

std::wstring FileNameFromPath(const std::wstring& path) {
  auto found = path.rfind(L"\\");
  return found == std::string::npos ? L"" : path.substr(found + 1);
}

std::wstring ToUpperCase(const std::wstring& str) {
  std::wstring result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), std::towupper);
  return result;
}

DesktopCapturer::SourceList GetProcessWindows(
    const DesktopCapturer::SourceList& sources,
    DWORD process,
    HWND window_to_exclude) {
  DesktopCapturer::SourceList result;
  std::copy_if(sources.begin(), sources.end(), std::back_inserter(result),
               [&](DesktopCapturer::Source source) {
                 const HWND source_hwnd = reinterpret_cast<HWND>(source.id);
                 return window_to_exclude != source_hwnd
                   && WindowProcess(source_hwnd) == process;
               });
  return result;
}

class FullScreenPowerPointHandler : public FullScreenApplicationHandler {
 public:
  FullScreenPowerPointHandler(DesktopCapturer::SourceId sourceId)
      : FullScreenApplicationHandler(sourceId) {}

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& current,
      int64_t timestamp) const override {
    if (current.empty())
      return 0;

    const HWND original_window = GetWindowId();
    const DWORD process = WindowProcess(original_window);

    DesktopCapturer::SourceList pp_windows =
        GetProcessWindows(current, process, original_window);

    if (pp_windows.empty())
      return 0;

    const std::wstring original_document =
        GetDocumentTitle(original_window, GetWindowType(original_window));

    for (const auto& source : pp_windows) {
      HWND window = reinterpret_cast<HWND>(source.id);
      const auto type = GetWindowType(window);
      if (type != WindowType::kSlideShow)
        continue;

      const std::wstring document = GetDocumentTitle(window, type);
      if (document != original_document)
        continue;

      return source.id;
    }

    return 0;
  }

 private:
  enum class WindowType { kEditor, kSlideShow, kOther };

  WindowType GetWindowType(HWND window) const {
    if (IsEditorlWindow(window))
      return WindowType::kEditor;
    else if (IsSlideShowWindow(window))
      return WindowType::kSlideShow;
    else
      return WindowType::kOther;
  }

  std::wstring GetDocumentTitle(HWND window, WindowType windowType) const {
    const std::wstring title = WindowText(window);
    const std::wstring separator = L" - ";

    if (windowType == WindowType::kEditor) {
      const std::wstring::size_type position = title.find(separator);
      return wstring_trim(title.substr(0, position));
    } else if (windowType == WindowType::kSlideShow) {
      const std::wstring::size_type left_position = title.find(separator);
      const std::wstring::size_type right_position = title.rfind(separator);
      if (right_position > left_position + separator.length()) {
        const std::wstring::size_type result_length =
            right_position - left_position - separator.length();
        const std::wstring document =
            title.substr(left_position + separator.length(), result_length);
        return wstring_trim(document);
      } else {
        const std::wstring document = title.substr(
            left_position + separator.length(), std::wstring::npos);
        return wstring_trim(document);
      }
    } else {
      return title;
    }
  }

  bool IsEditorlWindow(HWND window) const {
    WCHAR screen_class[] = L"PPTFrameClass";
    constexpr int screen_class_length = arraysize(screen_class) - 1;
    constexpr size_t kBufferLength = 256;
    WCHAR buffer[kBufferLength];
    const int length = ::GetClassNameW(window, buffer, kBufferLength);
    if (length != screen_class_length)
      return false;
    return wcsncmp(buffer, screen_class, length) == 0;
  }

  bool IsSlideShowWindow(HWND window) const {
    const LONG style = ::GetWindowLong(window, GWL_STYLE);
    const bool min_box = WS_MINIMIZEBOX & style;
    const bool max_box = WS_MAXIMIZEBOX & style;
    return !min_box && !max_box;
  }

  HWND GetWindowId() const { return reinterpret_cast<HWND>(GetSourceId()); }
};

std::wstring GetPathByWindowId(HWND window_id) {
  const DWORD dwProcessId = WindowProcess(window_id);
  const HANDLE processHandle =
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
  if (processHandle == NULL)
    return L"";
  DWORD path_len = MAX_PATH;
  WCHAR path[MAX_PATH];
  std::wstring result;
  if (::QueryFullProcessImageName(processHandle, 0, path, &path_len))
    result = std::wstring(path, path_len);
  else
    RTC_LOG_GLE(LS_ERROR) << "QueryFullProcessImageName failed.";

  ::CloseHandle(processHandle);
  return result;
}

}  // namespace

std::unique_ptr<FullScreenApplicationHandler>
CreateFullScreenWinApplicationHandler(DesktopCapturer::SourceId source_id) {
  std::unique_ptr<FullScreenApplicationHandler> result;
  std::wstring exe_path = GetPathByWindowId(reinterpret_cast<HWND>(source_id));
  std::wstring file_name = ToUpperCase(FileNameFromPath(exe_path));

  if (file_name == L"POWERPNT.EXE") {
    result = std::make_unique<FullScreenPowerPointHandler>(source_id);
  }

  return result;
}

}  // namespace webrtc
