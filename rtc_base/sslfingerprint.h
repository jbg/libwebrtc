/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSLFINGERPRINT_H_
#define RTC_BASE_SSLFINGERPRINT_H_

#include <string>

#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sslidentity.h"

namespace rtc {

class SSLCertificate;

struct SSLFingerprint {
  static std::unique_ptr<SSLFingerprint> Create(
      const std::string& algorithm,
      const rtc::SSLIdentity& identity);

  static std::unique_ptr<SSLFingerprint> Create(
      const std::string& algorithm,
      const rtc::SSLCertificate& cert);

  static std::unique_ptr<SSLFingerprint> CreateFromRfc4572(
      const std::string& algorithm,
      const std::string& fingerprint);

  // Creates a fingerprint from a certificate, using the same digest algorithm
  // as the certificate's signature.
  static std::unique_ptr<SSLFingerprint> CreateFromCertificate(
      const RTCCertificate& cert);

  SSLFingerprint(const std::string& algorithm,
                 ArrayView<const uint8_t> digest_view);

  SSLFingerprint(const SSLFingerprint& from);

  bool operator==(const SSLFingerprint& other) const;

  std::string GetRfc4572Fingerprint() const;

  std::string ToString() const;

  std::string algorithm;
  rtc::CopyOnWriteBuffer digest;
};

}  // namespace rtc

#endif  // RTC_BASE_SSLFINGERPRINT_H_
