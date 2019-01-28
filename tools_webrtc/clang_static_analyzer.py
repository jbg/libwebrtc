#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Invoke clang static analyzer.

Usage: clang_static_analyzer.py file.cc

NB: This was adapted from //build/toolchain/clang_static_analyzer_wrapper.py,
    but differs in several ways:
      * Flags, defines and include paths are inferred automatically.
      * No compilation step.
      * The set of enabled checkers may be different.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
#pylint: disable=relative-import
from presubmit_checks_lib.build_helpers import GetCompilationCommand, ValidateCC

# Flags used to enable analysis for Clang invocations.
ANALYZER_ENABLE_FLAGS = [
    '--analyze',
]

# Flags used to configure the analyzer's behavior.
ANALYZER_OPTION_FLAGS = [
    '-fdiagnostics-show-option',
    '-analyzer-opt-analyze-nested-blocks',
    '-analyzer-output=text',
    '-analyzer-config',
    'suppress-c++-stdlib=true',

# List of checkers to execute.
# The full list of checkers can be found at
# https://clang-analyzer.llvm.org/available_checks.html.
    '-analyzer-checker=cplusplus',
    '-analyzer-checker=core',
    '-analyzer-checker=unix',
    '-analyzer-checker=deadcode',
]


# Prepends every element of a list |args| with |token|.
# e.g. ['-analyzer-foo', '-analyzer-bar'] => ['-Xanalyzer', '-analyzer-foo',
#                                             '-Xanalyzer', '-analyzer-bar']
def InterleaveArgs(args, token):
  return list(sum(zip([token] * len(args), args), ()))


def Process(filepath):
  # Build directory is needed to gather compilation flags.
  # Create a temporary one (instead of reusing an existing one)
  # to keep the CLI simple and unencumbered.
  out_dir = tempfile.mkdtemp('static_analyzer')

  try:
    # We don't set 'use_clang_static_analyzer' to true, since we're
    # doing our own wrapping anyway.
    # We use debug configuration so that the analyzer can pick up
    # assertions to reduce false positives.
    command = GetCompilationCommand(filepath, ['--args=is_debug=true'], out_dir)
    # Trigger sanitizer with its configuration.
    command += ANALYZER_ENABLE_FLAGS + InterleaveArgs(ANALYZER_OPTION_FLAGS,
                                                      '-Xanalyzer')
    print "Running: %s" % ' '.join(command)
    # Run from build dir so that relative paths are correct.
    p = subprocess.Popen(command, cwd=out_dir,
                         stdout=sys.stdout, stderr=sys.stderr)
    p.communicate()
    return p.returncode  # NB: the analyzer returns 0 even if issues are found.
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)


def Main():
  description = (
    "Run clang static analyzer on single cc file.\n"
    "Use flags, defines and include paths as in default debug build.")
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('filepath',
                      help='Specifies the path of the .cc file to analyze.',
                      type=ValidateCC)
  return Process(parser.parse_args().filepath)


if __name__ == '__main__':
  sys.exit(Main())
