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
#include <algorithm>
#include <functional>
#include "absl/strings/match.h"
#include "modules/desktop_capture/mac/window_list_utils.h"

namespace webrtc {
namespace {

class FullScreenMacApplicationHandler : public FullScreenApplicationHandler {
 public:
  using TitlePredicate = std::function<bool(const std::string&, const std::string&)>;

  FullScreenMacApplicationHandler(DesktopCapturer::SourceId sourceId,
                                  TitlePredicate title_predicate)
      : FullScreenApplicationHandler(sourceId), title_predicate_(title_predicate) {}

  WindowId FindFullScreenWindowWithSamePid(const DesktopCapturer::SourceList& source_list) const {
    const auto original_window = GetSourceId();
    const std::string title = GetWindowTitle(original_window);

    // We can ignore any windows with empty titles cause regardless type of application it's
    // impossible to verify that full screen window and original window are related to the same
    // document.
    if (title.empty()) return kCGNullWindowID;

    const int pid = GetWindowOwnerPid(original_window);

    MacDesktopConfiguration desktop_config =
        MacDesktopConfiguration::GetCurrent(MacDesktopConfiguration::TopLeftOrigin);

    const auto it = std::find_if(
        source_list.begin(), source_list.end(), [&](const DesktopCapturer::Source& src) {
          if (src.id == original_window) return false;

          if (GetWindowOwnerPid(src.id) != pid) return false;

          const std::string window_title = GetWindowTitle(src.id);

          if (window_title.empty()) return false;

          if (title_predicate_ && !title_predicate_(title, window_title)) return false;

          return IsWindowFullScreen(desktop_config, src.id);
        });

    return it != source_list.end() ? it->id : 0;
  }

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& source_list) const override {
    return IsWindowOnScreen(GetSourceId()) ? 0 : FindFullScreenWindowWithSamePid(source_list);
  }

 private:
  TitlePredicate title_predicate_;
};

bool equal_title_predicate(const std::string& original_title, const std::string& title) {
  return original_title == title;
}

bool slide_show_title_predicate(const std::string& original_title, const std::string& title) {
  if (title.find(original_title) == std::string::npos) return false;

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
    if (absl::StartsWith(title, pp_slide_title)) return true;
  }
  return false;
}

}  // namespace

std::unique_ptr<FullScreenApplicationHandler> CreateFullScreenMacApplicationHandler(
    DesktopCapturer::SourceId sourceId) {
  std::unique_ptr<FullScreenApplicationHandler> result;
  int pid = GetWindowOwnerPid(sourceId);
  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  int path_length = proc_pidpath(pid, buffer, sizeof(buffer));
  if (path_length > 0) {
    const char* last_slash = strrchr(buffer, '/');
    const std::string name{last_slash ? last_slash + 1 : buffer};
    FullScreenMacApplicationHandler::TitlePredicate predicate = nullptr;
    if (name.find("Google Chrome") == 0 || name == "Chromium") {
      predicate = equal_title_predicate;
    } else if (name == "Microsoft PowerPoint") {
      predicate = slide_show_title_predicate;
    } else if (name == "Keynote") {
      predicate = equal_title_predicate;
    }

    if (predicate) {
      result.reset(new FullScreenMacApplicationHandler(sourceId, predicate));
    }
  }

  return result;
}

}  // namespace webrtc
