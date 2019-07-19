/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "rtc_tools/frame_editing/frame_editing_lib.h"

ABSL_FLAG(std::string, in_path, "", "Path and filename to the input file");
ABSL_FLAG(int32_t,
          width,
          -1,
          "Width in pixels of the frames in the input file");
ABSL_FLAG(int32_t,
          height,
          -1,
          "Height in pixels of the frames in the input file");
ABSL_FLAG(int32_t, f, -1, "First frame to process");
ABSL_FLAG(int32_t,
          interval,
          -1,
          "Interval specifies with what ratio the number of frames should be "
          "increased or decreased with");
ABSL_FLAG(int32_t, l, -1, "Last frame to process");
ABSL_FLAG(std::string,
          out_path,
          "output.yuv",
          "The output file to which frames are written");

// A command-line tool to edit a YUV-video (I420 sub-sampled).
int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "Deletes a series of frames in a yuv file. "
      "Only I420 is supported!\n"
      "Example usage:\n"
      "./frame_editor --in_path=input.yuv --width=320"
      " --height=240 --f=60 --interval=1 --l=120"
      " --out_path=edited_clip.yuv\n");
  absl::ParseCommandLine(argc, argv);

  const std::string in_path = absl::GetFlag(FLAGS_in_path);
  int width = absl::GetFlag(FLAGS_width);
  int height = absl::GetFlag(FLAGS_height);
  int first_frame_to_cut = absl::GetFlag(FLAGS_f);
  int interval = absl::GetFlag(FLAGS_interval);
  int last_frame_to_cut = absl::GetFlag(FLAGS_l);

  const std::string out_path = absl::GetFlag(FLAGS_out_path);

  if (in_path.empty()) {
    fprintf(stderr, "You must specify a file to edit\n");
    return -1;
  }

  if (first_frame_to_cut <= 0 || last_frame_to_cut <= 0) {
    fprintf(stderr, "Error: You must specify which frames to cut!\n");
    return -2;
  }

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -3;
  }
  return webrtc::EditFrames(in_path, width, height, first_frame_to_cut,
                            interval, last_frame_to_cut, out_path);
}
