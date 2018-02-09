# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Configuration specification and helpers for the QA code.
"""

class Param(object):
  """Contains the names of all supported configuration parameters, as well as
  default configurations.
  """

  # The name of the configuration to use when storing data.
  NAME = 'name'

  # Specifies an offset to apply to near-end audio when mixing in the echo
  # signal.
  SPEECH_TO_ECHO_DB = 'speech_to_echo_db'

  # A default configuration, used if none is specified by user input.
  DEFAULT_CONFIG = {NAME:'default'}
