/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_EXPLICIT_KEY_VALUE_CONFIG_H_
#define TEST_EXPLICIT_KEY_VALUE_CONFIG_H_

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "api/webrtc_key_value_config.h"
#include "test/field_trial.h"

namespace webrtc {
namespace test {

class ExplicitKeyValueConfig : public WebRtcKeyValueConfig {
 public:
  ExplicitKeyValueConfig();
  explicit ExplicitKeyValueConfig(const std::string& s);
  ExplicitKeyValueConfig(const ExplicitKeyValueConfig* parent,
                         const std::string& s);

  std::string Lookup(absl::string_view key) const override;

 private:
  const ExplicitKeyValueConfig* parent_;
  std::map<std::string, std::string> key_value_map_;
  std::unique_ptr<ScopedFieldTrials> scoped_field_trials_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_EXPLICIT_KEY_VALUE_CONFIG_H_
