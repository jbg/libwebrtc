# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import sys

def GenerateUmbrellaHeader():
  parser = argparse.ArgumentParser(description='Generate umbrella header')
  parser.add_argument("-o", "--out", type=str, help="Output file.")
  parser.add_argument("-s", "--sources", default=[], type=str, nargs='+',
                      help="Headers to include.")

  args = parser.parse_args()

  outfile = open(args.out, "w")

  outfile.write("""/*
   *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
   *
   *  Use of this source code is governed by a BSD-style license
   *  that can be found in the LICENSE file in the root of the source
   *  tree. An additional intellectual property rights grant can be found
   *  in the file PATENTS.  All contributing project authors may
   *  be found in the AUTHORS file in the root of the source tree.
   */\n
  """)

  for s in args.sources:
    outfile.write("#import <{}>\n".format(s))

  outfile.close()

if __name__ == '__main__':
  sys.exit(GenerateUmbrellaHeader())
