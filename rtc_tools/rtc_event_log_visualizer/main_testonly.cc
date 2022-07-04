/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdio>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"

namespace {
absl::string_view kDefaultFile =
    "audio_processing/conversational_speech/EN_script2_F_sp2_B1";
absl::string_view kDefaultFileExt = "wav";
}  // namespace

int main(int argc, char** argv) {
  std::string wav_path =
      webrtc::test::ResourcePath(kDefaultFile, kDefaultFileExt);
  std::vector<std::string> cmd;
  for (int i = 0; i < argc; ++i) {
    RTC_CHECK(!absl::StartsWith(argv[i], "--wav_filename"))
        << "Calling `event_log_visualizer_testonly` with --wav_filename, "
           "please use `event_log_visualizer` instead.";
    if (i == 0) {
      cmd.push_back(absl::StrReplaceAll(argv[i], {{"_testonly", ""}}));
    } else {
      cmd.push_back(argv[i]);
    }
  }
  cmd.push_back("--wav_filename");
  cmd.push_back(wav_path);
  std::string cmd_to_execute = absl::StrJoin(cmd, " ");

  FILE* f = reinterpret_cast<FILE*>(popen(cmd_to_execute.c_str(), "r"));
  char c = 0;

  if (f == NULL) {
    perror("popen() failed.");
    exit(EXIT_FAILURE);
  }

  while (std::fread(&c, sizeof c, 1, f)) {
    printf("%c", c);
  }

  pclose(f);
  return EXIT_SUCCESS;
}
