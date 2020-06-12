/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/explicit_key_value_config.h"

#include <vector>

#include "api/transport/webrtc_key_value_config.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace test {

ExplicitKeyValueConfig::ExplicitKeyValueConfig(const std::string& s) {
  std::vector<std::string> fields = absl::StrSplit(s, '/');
  // Expect an even number of '/', so an odd number of fields where the last one
  // is empty.
  RTC_CHECK(fields.size() % 2 == 1);
  RTC_CHECK(fields[fields.size() - 1].empty());
  for (size_t i = 0; i + 1 < fields.size(); i += 2) {
    RTC_CHECK(!fields[i].empty());
    RTC_CHECK(!fields[i + 1].empty());
    key_value_map_[fields[i]] = fields[i + 1];
  }
}

std::string ExplicitKeyValueConfig::Lookup(absl::string_view key) const {
  std::string key_string(key);
  auto it = key_value_map_.find(key_string);
  if (it != key_value_map_.end())
    return it->second;
  return "";
}

}  // namespace test
}  // namespace webrtc
