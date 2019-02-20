/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"

namespace webrtc {
namespace test {

class SsrcSwitchDetector;
class NetEqStatsGetter;
class NetEqStatsPlotter;

// Note that the NetEqTestFactory needs to be alive when the NetEqTest object is
// used for a simulation.
class NetEqTestFactory {
 public:
  NetEqTestFactory();
  ~NetEqTestFactory();
  struct Config {
    Config();
    Config(const Config& other);
    ~Config();
    // RTP payload type for PCM-u.
    int pcmu = 0;
    // RTP payload type for PCM-a.
    int pcma = 8;
    // RTP payload type for iLBC.
    int ilbc = 102;
    // RTP payload type for iSAC.
    int isac = 103;
    // RTP payload type for iSAC-swb (32 kHz).
    int isac_swb = 104;
    // RTP payload type for Opus.
    int opus = 111;
    // RTP payload type for PCM16b-nb (8 kHz).
    int pcm16b = 93;
    // RTP payload type for PCM16b-wb (16 kHz).
    int pcm16b_wb = 94;
    // RTP payload type for PCM16b-swb32 (32 kHz).
    int pcm16b_swb32 = 95;
    // RTP payload type for PCM16b-swb48 (48 kHz).
    int pcm16b_swb48 = 96;
    // RTP payload type for G.722.
    int g722 = 9;
    // RTP payload type for AVT/DTMF (8 kHz).
    int avt = 106;
    // RTP payload type for AVT/DTMF (16 kHz).
    int avt_16 = 114;
    // RTP payload type for AVT/DTMF (32 kHz).
    int avt_32 = 115;
    // RTP payload type for AVT/DTMF (48 kHz).
    int avt_48 = 116;
    // RTP payload type for redundant audio (RED).
    int red = 117;
    // RTP payload type for comfort noise (8 kHz).
    int cn_nb = 13;
    // RTP payload type for comfort noise (16 kHz).
    int cn_wb = 98;
    // RTP payload type for comfort noise (32 kHz).
    int cn_swb32 = 99;
    // RTP payload type for comfort noise (48 kHz).
    int cn_swb48 = 100;
    // A PCM file that will be used to populate dummy RTP packets.
    std::string replacement_audio_file;
    // Only use packets with this SSRC.
    absl::optional<uint32_t> ssrc_filter;
    // Extension ID for audio level (RFC 6464).
    int audio_level = 1;
    // Extension ID for absolute sender time.
    int abs_send_time = 3;
    // Extension ID for transport sequence number.
    int transport_seq_no = 5;
    // Extension ID for video content type.
    int video_content_type = 7;
    // Extension ID for video timing.
    int video_timing = 8;
    // Generate a matlab script for plotting the delay profile.
    bool matlabplot = false;
    // Generates a python script for plotting the delay profile.
    bool pythonplot = false;
    // Generates a text log describing the simulation on a step-by-step basis.
    bool textlog = false;
    // Prints concealment events.
    bool concealment_events = false;
    // Maximum allowed number of packets in the buffer.
    int max_nr_packets_in_buffer = 50;
    // Enables jitter buffer fast accelerate.
    bool enable_fast_accelerate = false;
  };

  std::unique_ptr<NetEqTest> InitializeTest(std::string input_filename,
                                            std::string output_filename,
                                            const Config& config);

 private:
  std::unique_ptr<SsrcSwitchDetector> ssrc_switch_detector_;
  std::unique_ptr<NetEqStatsPlotter> stats_plotter_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_
