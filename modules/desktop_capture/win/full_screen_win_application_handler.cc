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
#include <algorithm>
#include <memory>
#include <string>
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"  // For RTC_LOG_GLE
#include "rtc_base/string_utils.h"

namespace webrtc {
namespace {

std::string WindowText(HWND window) {
  size_t len = ::GetWindowTextLength(window);
  if (len == 0)
    return "";

  std::vector<wchar_t> buffer(len + 1, 0);
  size_t copied = ::GetWindowTextW(window, buffer.data(), buffer.size());
  return copied > 0 ? rtc::ToUtf8(buffer.data(), copied) : "";
}

DWORD WindowProcessId(HWND window) {
  DWORD dwProcessId = 0;
  ::GetWindowThreadProcessId(window, &dwProcessId);
  return dwProcessId;
}

std::wstring FileNameFromPath(const std::wstring& path) {
  auto found = path.rfind(L"\\");
  return found == std::string::npos ? path : path.substr(found + 1);
}

std::wstring ToUpperCase(const std::wstring& str) {
  std::wstring result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), std::towupper);
  return result;
}

// Returns windows which belong to given process id
// |sources| is a full list of available windows
// |processId| is a process identifier (window owner)
// |window_to_exclude| is a window to be exluded from result
DesktopCapturer::SourceList GetProcessWindows(
    const DesktopCapturer::SourceList& sources,
    DWORD processId,
    HWND window_to_exclude) {
  DesktopCapturer::SourceList result;
  std::copy_if(sources.begin(), sources.end(), std::back_inserter(result),
               [&](DesktopCapturer::Source source) {
                 const HWND source_hwnd = reinterpret_cast<HWND>(source.id);
                 return window_to_exclude != source_hwnd
                   && WindowProcessId(source_hwnd) == processId;
               });
  return result;
}

class FullScreenPowerPointHandler : public FullScreenApplicationHandler {
 public:
  FullScreenPowerPointHandler(DesktopCapturer::SourceId sourceId)
      : FullScreenApplicationHandler(sourceId) {}

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& window_list,
      int64_t timestamp) const override {
    if (window_list.empty())
      return 0;

    HWND original_window = reinterpret_cast<HWND>(GetSourceId());
    DWORD process_id = WindowProcessId(original_window);

    DesktopCapturer::SourceList powerpoint_windows =
        GetProcessWindows(window_list, process_id, original_window);

    if (powerpoint_windows.empty())
      return 0;

    if (GetWindowType(original_window) != WindowType::kEditor)
      return 0;

    const auto original_document = GetDocumentFromEditorTitle(original_window);

    for (const auto& source : powerpoint_windows) {
      HWND window = reinterpret_cast<HWND>(source.id);

      // Looking for slide show window for the same document
      if (GetWindowType(window) != WindowType::kSlideShow ||
          GetDocumentFromSlideShowTitle(window) != original_document)
        continue;

      return source.id;
    }

    return 0;
  }

 private:
  enum class WindowType { kEditor, kSlideShow, kOther };

  WindowType GetWindowType(HWND window) const {
    if (IsEditorWindow(window))
      return WindowType::kEditor;
    else if (IsSlideShowWindow(window))
      return WindowType::kSlideShow;
    else
      return WindowType::kOther;
  }

  constexpr static char kSeparator[] = " - ";

  std::string GetDocumentFromEditorTitle(HWND window) const {
    auto title = WindowText(window);
    auto position = title.find(kSeparator);
    return rtc::string_trim(title.substr(0, position));
  }

  std::string GetDocumentFromSlideShowTitle(HWND window) const {
    std::string title = WindowText(window);
    auto left_pos = title.find(kSeparator);
    auto right_pos = title.rfind(kSeparator);
    constexpr size_t separator_len = arraysize(kSeparator) - 1;
    if (left_pos == std::string::npos || right_pos == std::string::npos)
      return title;

    if (right_pos > left_pos + separator_len) {
      auto result_len = right_pos - left_pos - separator_len;
      auto document = title.substr(left_pos + separator_len, result_len);
      return rtc::string_trim(document);
    } else {
      auto document =
          title.substr(left_pos + separator_len, std::wstring::npos);
      return rtc::string_trim(document);
    }
  }

  bool IsEditorWindow(HWND window) const {
    constexpr WCHAR screen_class[] = L"PPTFrameClass";
    constexpr int screen_class_length = arraysize(screen_class) - 1;

    // It's enough to have buffer that size to check
    // if window class is equal to |screen_class|
    constexpr size_t kBufferLength = arraysize(screen_class) + 1;
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
};

std::wstring GetPathByWindowId(HWND window_id) {
  DWORD process_id = WindowProcessId(window_id);
  HANDLE process =
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == NULL)
    return L"";
  DWORD path_len = MAX_PATH;
  WCHAR path[MAX_PATH];
  std::wstring result;
  if (::QueryFullProcessImageName(process, 0, path, &path_len))
    result = std::wstring(path, path_len);
  else
    RTC_LOG_GLE(LS_ERROR) << "QueryFullProcessImageName failed.";

  ::CloseHandle(process);
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
