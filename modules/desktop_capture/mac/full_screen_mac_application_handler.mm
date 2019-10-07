/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/full_screen_mac_application_handler.h"
#include <libproc.h>
#include <functional>
#include "rtc_base/logging.h"
#include "absl/strings/match.h"
#include "modules/desktop_capture/mac/window_list_utils.h"

namespace webrtc {
namespace {

class FullScreenMacApplicationHandler : public FullScreenApplicationHandler {
 public:
  using TitlePredicate =
      std::function<bool(const std::string&, const std::string&)>;

  FullScreenMacApplicationHandler(TitlePredicate title_predicate)
      : title_predicate_(title_predicate) {}

  WindowId GetWindowId() const { return GetSourceId(); }

  WindowId FindFullScreenWindowWithSamePid() const {
    const int pid = GetWindowOwnerPid(GetWindowId());
    const std::string title = GetWindowTitle(GetWindowId());

    if (title.empty()) return kCGNullWindowID;

    // Only get on screen, non-desktop windows.
    CFArrayRef window_array = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);

    if (!window_array) return kCGNullWindowID;

    CGWindowID full_screen_window = kCGNullWindowID;

    MacDesktopConfiguration desktop_config =
        MacDesktopConfiguration::GetCurrent(MacDesktopConfiguration::TopLeftOrigin);

    // Check windows to make sure they have an id, title, and use window layer
    // other than 0.
    CFIndex count = CFArrayGetCount(window_array);
    for (CFIndex i = 0; i < count; ++i) {
      CFDictionaryRef window =
          reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(window_array, i));

      CGWindowID window_id = webrtc::GetWindowId(window);
      if (window_id == kNullWindowId) continue;

      if (GetWindowOwnerPid(window) != pid) continue;

      const std::string window_title = GetWindowTitle(window);
      if (title_predicate_ && !title_predicate_(title, window_title))
        continue;

      if (IsWindowFullScreen(desktop_config, window)) {
        full_screen_window = window_id;
        break;
      }
    }
    CFRelease(window_array);
    return full_screen_window;
  }

  bool CanHandleFullScreen() const override { return true; }

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& previousWindowList,
      const DesktopCapturer::SourceList& currentWindowList) const override {
    const WindowId result = FindFullScreenWindowWithSamePid();

    if (result == kNullWindowId) return 0;

    for (const auto& window : previousWindowList) {
      if (window.id != result) continue;

      RTC_LOG(LS_WARNING) << "The full-screen window exists in the list.";
      return 0;
    }

    return result;
  }

 private:
  TitlePredicate title_predicate_;
};

bool equal_title_predicate(const std::string& original_title,
                           const std::string& title) {
  return original_title == title;
}

bool slide_show_title_predicate(const std::string& original_title,
                                const std::string& title) {
  if (title.find(original_title) == std::string::npos)
    return false;

  const char* pp_slide_titles[] = {u8"PowerPoint-Bildschirmpräsentation",
                                   u8"Προβολή παρουσίασης PowerPoint",
                                   u8"PowerPoint スライド ショー",
                                   u8"PowerPoint Slide Show",
                                   u8"PowerPoint 幻灯片放映",
                                   u8"Presentación de PowerPoint",
                                   u8"PowerPoint-slideshow",
                                   u8"Presentazione di PowerPoint",
                                   u8"Prezentácia programu PowerPoint",
                                   u8"Apresentação do PowerPoint",
                                   u8"PowerPoint-bildspel",
                                   u8"Prezentace v aplikaci PowerPoint",
                                   u8"PowerPoint 슬라이드 쇼",
                                   u8"PowerPoint-lysbildefremvisning",
                                   u8"PowerPoint-vetítés",
                                   u8"PowerPoint Slayt Gösterisi",
                                   u8"Pokaz slajdów programu PowerPoint",
                                   u8"PowerPoint 投影片放映",
                                   u8"Демонстрация PowerPoint",
                                   u8"Diaporama PowerPoint",
                                   u8"PowerPoint-diaesitys",
                                   u8"Peragaan Slide PowerPoint",
                                   u8"PowerPoint-diavoorstelling",
                                   u8"การนำเสนอสไลด์ PowerPoint",
                                   u8"Apresentação de slides do PowerPoint",
                                   u8"הצגת שקופיות של PowerPoint",
                                   u8"عرض شرائح في PowerPoint"};

  for (const char* pp_slide_title : pp_slide_titles) {
    if (absl::StartsWith(title, pp_slide_title))
      return true;
  }
  return false;
}

}  // namespace

std::unique_ptr<FullScreenApplicationHandler>
CreateFullScreenMacApplicationHandler(DesktopCapturer::SourceId sourceId) {
  std::unique_ptr<FullScreenApplicationHandler> result;
  int pid = GetWindowOwnerPid(sourceId);
  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  int path_length = proc_pidpath(pid, buffer, sizeof(buffer));
  if (path_length > 0) {
    const char* last_slash = strrchr(buffer, '/');
    const std::string name{last_slash ? last_slash + 1 : buffer};
    if (name.find("Google Chrome") == 0 || name == "Chromium") {
      result.reset(new FullScreenMacApplicationHandler(equal_title_predicate));
    } else if (name == "Microsoft PowerPoint") {
      result.reset(
          new FullScreenMacApplicationHandler(slide_show_title_predicate));
    } else if (name == "Keynote") {
      result.reset(new FullScreenMacApplicationHandler(equal_title_predicate));
    }
  }

  if (result == nullptr) {
    result.reset(new FullScreenApplicationHandler());
  }
  result->SetSourceId(sourceId);
  return result;
}

}  // namespace webrtc
