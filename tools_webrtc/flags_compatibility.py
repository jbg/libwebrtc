#!/usr/bin/env python

# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import logging
import os
import subprocess
import sys

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('build_dir',
                      help='Path to the build directory (e.g. out/Release).')
  parser.add_argument('binary_to_run',
                      help='The binary to run (e.g. webrtc_perf_tests).')
  parser.add_argument('--isolated-script-test-output')
  parser.add_argument('--isolated-script-test-perf-output')
  args, unrecognized_args = parser.parse_known_args()

  test_command = [os.path.join(args.build_dir, args.binary_to_run)]
  if args.isolated_script_test_output:
    test_command += ['--isolated_script_test_output',
                     args.isolated_script_test_output]
  if args.isolated_script_test_perf_output:
    test_command += ['--isolated_script_test_perf_output',
                     args.isolated_script_test_perf_output]
  test_command += unrecognized_args
  logging.info('Running %r', test_command)

  test_process = subprocess.Popen(test_command, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT)
  stdout, _ = test_process.communicate()
  print stdout


if __name__ == '__main__':
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
