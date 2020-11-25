/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "test/ios/coverage_util_ios.h"

int main(int argc, char* argv[]) {
  NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
  int32_t major_version = static_cast<int32_t>(version.majorVersion);
  if (major_version < 14) {
    rtc::test::ConfigureCoverageReportPath();

    @autoreleasepool {
      return UIApplicationMain(argc, argv, nil, nil);
    }
  }
  return 0;
}
