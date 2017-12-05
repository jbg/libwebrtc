/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)

#import <Foundation/Foundation.h>

#include "sdk/objc/Framework/Classes/Common/helpers.h"

namespace webrtc {
namespace test {

using webrtc::ios::NSStringFromStdString;
using webrtc::ios::StdStringFromNSString;

std::string IOSDocumentDirectory() {
  NSString* fileName = @"perf_result.json";
  NSArray<NSString*>* outputDirectories = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  if ([outputDirectories count] == 0) {
    return ".";
  }
  NSString* outputPath =
      [outputDirectories[0] stringByAppendingPathComponent:fileName];
  return StdStringFromNSString(outputPath);
}

}  // namespace test
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)
