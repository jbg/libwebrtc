/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ASYNCRESOLVERFACTORY_H_
#define API_ASYNCRESOLVERFACTORY_H_

#include "rtc_base/asyncresolverinterface.h"

namespace rtc {

// An abstract factory for creating AsyncResolverInterfaces.
class AsyncResolverFactory {
 public:
  AsyncResolverFactory() = default;
  virtual ~AsyncResolverFactory() = default;

  // The returned object is responsible for deleting itself after address
  // resolution has completed.
  virtual AsyncResolverInterface* Create() = 0;
};

}  // namespace rtc

#endif  // API_ASYNCRESOLVERFACTORY_H_
