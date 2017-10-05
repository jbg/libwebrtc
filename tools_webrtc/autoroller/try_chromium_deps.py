#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to try a WebRTC build with chromium/src rolled from a local folder."""

import argparse
import logging
import sys
import os
import subprocess
import shutil

from roll_deps import (CHECKOUT_SRC_DIR, CHROMIUM_SRC_URL,
                       BuildDepsentryDict, ParseLocalDepsFile,
                       _IsTreeClean)


def _Ensure(condition, *logging_args):
  if not condition:
    logging.error(*logging_args)
    sys.exit(1)


def main(argv):
  p = argparse.ArgumentParser()
  p.add_argument('--webrtc-checkout', default=CHECKOUT_SRC_DIR,
                 help=('WebRTC Git checkout (src directory) to modify.'))
  p.add_argument('chromium_checkout', metavar='CHROMIUM_CHECKOUT',
                 help=('Chromium Git checkout (src directory) to copy from.'))
  p.add_argument('--skip-checks', action='store_true',
                 help=('Skip expensive sanity checks.'))
  p.add_argument('-v', '--verbose', action='store_true',
                 help='Be extra verbose in printing of log messages.')
  opts = p.parse_args(argv)

  if opts.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.INFO)

  deps_filename = os.path.join(CHECKOUT_SRC_DIR, 'DEPS')
  raw_deps = ParseLocalDepsFile(deps_filename)
  current_cr_rev = raw_deps['vars']['chromium_revision']
  deps = BuildDepsentryDict(raw_deps)

  dirs_to_copy = []

  for entry in deps.values():
    if entry.url.startswith(CHROMIUM_SRC_URL):
      _Ensure(entry.path.startswith('src/'),
              'The entry %r does not start with "src/"', entry)
      webrtc_subdir = entry.path[len('src/'):]

      chromium_subdir = entry.url[len(CHROMIUM_SRC_URL):].strip('/')
      _Ensure(webrtc_subdir == chromium_subdir,
              'The entry %r has non-matching directory name and URL', entry)
      dirs_to_copy.append(webrtc_subdir)

      dst = os.path.join(opts.webrtc_checkout, webrtc_subdir)
      subdir_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                              cwd=dst).strip()
      _Ensure(subdir_commit == entry.revision,
              'The directory %r does not match the specified revision %r. '
              'Try running `gclient sync`.',
              dst, subdir_commit)
      if not opts.skip_checks:
        _Ensure(_IsTreeClean(dst),
                'The directory %r has unstaged changes. ', dst)

  logging.info('Directories to sync: %r', dirs_to_copy)

  command = ['git', 'diff', '--name-only', current_cr_rev, '--'] + dirs_to_copy
  stdout = subprocess.check_output(command, cwd=opts.chromium_checkout)
  differing_files = stdout.splitlines()
  for fn in differing_files:
    logging.debug('Copying %r', fn)
    src = os.path.join(opts.chromium_checkout, fn)
    dst = os.path.join(opts.webrtc_checkout, fn)
    try:
      shutil.copy(src, dst)
    except IOError:
      if not os.path.isfile(src):
        logging.debug('Removing %r', dst)
        os.remove(dst)
      else:
        logging.debug('New directory %r', os.path.dirname(dst))
        os.makedirs(os.path.dirname(dst))
        shutil.copy(src, dst)
  logging.info('Synced %r files', len(differing_files))

  return bool(differing_files)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
