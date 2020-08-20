/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/openssl_utility.h"
#if defined(WEBRTC_WIN)
// Must be included first before openssl headers.
#include "rtc_base/win32.h"  // NOLINT
#endif                       // WEBRTC_WIN

#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/pool.h>
#else
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif
#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/openssl.h"
#include "rtc_base/openssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#ifndef WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS
#include "rtc_base/ssl_roots.h"
#endif  // WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS

namespace rtc {
namespace openssl {

// Holds various helper methods.
namespace {

#ifdef OPENSSL_IS_BORINGSSL
// Performs wildcard matching according to RFC5890, and with some added
// restrictions used by BoringSSL.
bool WildcardMatch(const std::string& host, const std::string& pattern) {
  // "- 1" in case wildcard matches zero characters.
  if (pattern.size() - 1 > host.size()) {
    return false;
  }
  size_t wildcard_pos = pattern.find('*');
  if (wildcard_pos == std::string::npos) {
    return absl::EqualsIgnoreCase(host, pattern);
  }
  // Can only have one wildcard character.
  if (pattern.find('*', wildcard_pos + 1) != std::string::npos) {
    return false;
  }
  // Don't perform wildcard matching with international names.
  if (absl::StartsWith(host, "xn--") || absl::StartsWith(pattern, "xn--")) {
    return false;
  }
  // Split into the parts before and after the wildcard.
  absl::string_view pattern_prefix(pattern);
  pattern_prefix.remove_suffix(pattern.size() - wildcard_pos);
  // Wildcard character must be part of leftmost label.
  if (pattern_prefix.find('.') != std::string::npos) {
    return false;
  }
  absl::string_view pattern_suffix(pattern);
  pattern_suffix.remove_prefix(wildcard_pos + 1);
  // Should be at least two dots after wildcard (e.g. "*.example.com", not
  // "*.com").
  if (std::count(pattern_suffix.begin(), pattern_suffix.end(), '.') < 2) {
    return false;
  }
  // Wildcard must be at start or end of label (e.g. "foo*.example.com" or
  // "*foo.example.com", not "f*o.example.com").
  if (!pattern_prefix.empty() || pattern_suffix[0] != '.') {
    return false;
  }
  // Split into the parts before and after the wildcard matched portion, and
  // the wildcard matched portion itself.
  absl::string_view host_prefix(host);
  host_prefix.remove_suffix(host.size() - wildcard_pos);
  absl::string_view host_suffix(host);
  host_suffix.remove_prefix(host.size() - pattern_suffix.size());
  absl::string_view host_wildcard_match(host);
  host_wildcard_match.remove_prefix(host_prefix.size());
  host_wildcard_match.remove_suffix(host_suffix.size());
  // If the wildcard makes up the entire first label, it must match at least
  // one character (e.g. "*.example.com" can't match ".example.com").
  if (host_prefix.empty() && host_suffix[0] == '.' &&
      host_wildcard_match.empty()) {
    return false;
  }
  // Wildcard can't match any '.'s (e.g. "*.example.com" can't match
  // "foo.bar.example.com")
  if (host_wildcard_match.find('.') != std::string::npos) {
    return false;
  }
  return absl::EqualsIgnoreCase(host_prefix, pattern_prefix) &&
         absl::EqualsIgnoreCase(host_suffix, pattern_suffix);
}

bool MatchSubjectAltName(CBS* extensions,
                         const std::string& host,
                         bool* san_present) {
  *san_present = false;
  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  CBS extension_sequence;
  if (!CBS_get_asn1(extensions, &extension_sequence, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  while (CBS_len(&extension_sequence)) {
    //    Extension  ::=  SEQUENCE  {
    CBS extension;
    if (!CBS_get_asn1(&extension_sequence, &extension, CBS_ASN1_SEQUENCE)) {
      return false;
    }
    //            extnID      OBJECT IDENTIFIER,
    CBS extension_oid;
    if (!CBS_get_asn1(&extension, &extension_oid, CBS_ASN1_OBJECT)) {
      return false;
    }
    // We're only interested in the subject alt name extension.
    static const uint8_t kSubjectAltName[] = {0x55, 0x1d, 0x11};
    if (CBS_len(&extension_oid) != sizeof(kSubjectAltName) ||
        0 != memcmp(kSubjectAltName, CBS_data(&extension_oid),
                    sizeof(kSubjectAltName))) {
      continue;
    }
    *san_present = true;
    //            critical    BOOLEAN DEFAULT FALSE,
    if (!CBS_get_optional_asn1(&extension, nullptr, nullptr,
                               CBS_ASN1_BOOLEAN)) {
      return false;
    }
    //            extnValue   OCTET STRING
    CBS extension_value;
    if (!CBS_get_asn1(&extension, &extension_value, CBS_ASN1_OCTETSTRING)) {
      return false;
    }
    // RFC 5280 section 4.2.1.6:
    // GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
    CBS general_names;
    if (!CBS_get_asn1(&extension_value, &general_names, CBS_ASN1_SEQUENCE)) {
      return false;
    }
    while (CBS_len(&general_names)) {
      CBS general_name;
      unsigned general_name_tag;
      if (!CBS_get_any_asn1(&general_names, &general_name, &general_name_tag)) {
        return false;
      }
      // Only interested in DNS name.
      // dNSName                         [2]     IA5String,
      if (general_name_tag != (0x2 | CBS_ASN1_CONTEXT_SPECIFIC)) {
        continue;
      }
      if (WildcardMatch(host, std::string(CBS_data(&general_name),
                                          CBS_data(&general_name) +
                                              CBS_len(&general_name)))) {
        return true;
      }
    }
    return false;
  }  // while (CBS_len(&extension_sequence))
  return false;
}

bool MatchSubjectName(CBS* subject_name, const std::string& host) {
  // RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
  CBS rdn_sequence;
  if (!CBS_get_asn1(subject_name, &rdn_sequence, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  while (CBS_len(&rdn_sequence)) {
    // RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue
    CBS rdn;
    if (!CBS_get_asn1(&rdn_sequence, &rdn, CBS_ASN1_SET)) {
      return false;
    }
    while (CBS_len(&rdn)) {
      // AttributeTypeAndValue ::= SEQUENCE {
      CBS type_and_value;
      if (!CBS_get_asn1(&rdn, &type_and_value, CBS_ASN1_SEQUENCE)) {
        return false;
      }
      // AttributeType ::= OBJECT IDENTIFIER
      CBS type;
      if (!CBS_get_asn1(&type_and_value, &type, CBS_ASN1_OBJECT)) {
        return false;
      }
      // We're only interested in the common name.
      static const uint8_t kCommonName[] = {0x55, 0x04, 0x03};
      if (CBS_len(&type) != sizeof(kCommonName) ||
          0 != memcmp(kCommonName, CBS_data(&type), sizeof(kCommonName))) {
        continue;
      }
      // AttributeValue ::= ANY -- DEFINED BY AttributeType
      unsigned common_name_tag;
      CBS common_name;
      if (!CBS_get_any_asn1(&type_and_value, &common_name, &common_name_tag)) {
        return false;
      }
      ASN1_STRING* common_name_string = nullptr;
      switch (common_name_tag) {
        case CBS_ASN1_T61STRING:
          common_name_string = M_ASN1_T61STRING_new();
          break;
        case CBS_ASN1_IA5STRING:
          common_name_string = M_ASN1_IA5STRING_new();
          break;
        case CBS_ASN1_PRINTABLESTRING:
          common_name_string = M_ASN1_PRINTABLESTRING_new();
          break;
        case CBS_ASN1_UTF8STRING:
          common_name_string = M_ASN1_UTF8STRING_new();
          break;
        case CBS_ASN1_UNIVERSALSTRING:
          common_name_string = M_ASN1_UNIVERSALSTRING_new();
          break;
        case CBS_ASN1_BMPSTRING:
          common_name_string = M_ASN1_BMPSTRING_new();
          break;
        default:
          break;
      }
      if (!common_name_string) {
        continue;
      }
      if (!ASN1_STRING_set(common_name_string, CBS_data(&common_name),
                           CBS_len(&common_name))) {
        return false;
      }
      int common_name_length;
      unsigned char* common_name_utf8;
      common_name_length =
          ASN1_STRING_to_UTF8(&common_name_utf8, common_name_string);
      ASN1_STRING_free(common_name_string);
      if (common_name_length < 0) {
        continue;
      }
      bool match = WildcardMatch(
          host,
          std::string(common_name_utf8, common_name_utf8 + common_name_length));
      OPENSSL_free(common_name_utf8);
      if (match) {
        return true;
      }
    }  // while (CBS_len(&rdn))
  }    // while (CBS_len(&rdn_sequence))
  return false;
}
#else  // OPENSSL_IS_BORINGSSL
void LogCertificates(SSL* ssl, X509* certificate) {
// Logging certificates is extremely verbose. So it is disabled by default.
#ifdef LOG_CERTIFICATES
  BIO* mem = BIO_new(BIO_s_mem());
  if (mem == nullptr) {
    RTC_DLOG(LS_ERROR) << "BIO_new() failed to allocate memory.";
    return;
  }

  RTC_DLOG(LS_INFO) << "Certificate from server:";
  X509_print_ex(mem, certificate, XN_FLAG_SEP_CPLUS_SPC, X509_FLAG_NO_HEADER);
  BIO_write(mem, "\0", 1);

  char* buffer = nullptr;
  BIO_get_mem_data(mem, &buffer);
  if (buffer != nullptr) {
    RTC_DLOG(LS_INFO) << buffer;
  } else {
    RTC_DLOG(LS_ERROR) << "BIO_get_mem_data() failed to get buffer.";
  }
  BIO_free(mem);

  const char* cipher_name = SSL_CIPHER_get_name(SSL_get_current_cipher(ssl));
  if (cipher_name != nullptr) {
    RTC_DLOG(LS_INFO) << "Cipher: " << cipher_name;
  } else {
    RTC_DLOG(LS_ERROR) << "SSL_CIPHER_DESCRIPTION() failed to get cipher_name.";
  }
#endif
}
#endif  // !defined(OPENSSL_IS_BORINGSSL)
}  // namespace

#ifdef OPENSSL_IS_BORINGSSL
bool ParseCertificate(CRYPTO_BUFFER* cert_buffer,
                      CBS* signature_algorithm_oid,
                      int64_t* expiration_time,
                      CBS* subject_name,
                      CBS* extensions) {
  CBS cbs;
  CRYPTO_BUFFER_init_CBS(cert_buffer, &cbs);

  //   Certificate  ::=  SEQUENCE  {
  CBS certificate;
  if (!CBS_get_asn1(&cbs, &certificate, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        tbsCertificate       TBSCertificate,
  CBS tbs_certificate;
  if (!CBS_get_asn1(&certificate, &tbs_certificate, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        signatureAlgorithm   AlgorithmIdentifier,
  CBS signature_algorithm;
  if (!CBS_get_asn1(&certificate, &signature_algorithm, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  if (!CBS_get_asn1(&signature_algorithm, signature_algorithm_oid,
                    CBS_ASN1_OBJECT)) {
    return false;
  }
  //        signatureValue       BIT STRING  }
  if (!CBS_get_asn1(&certificate, nullptr, CBS_ASN1_BITSTRING)) {
    return false;
  }

  // Now parse the inner TBSCertificate.
  //        version         [0]  EXPLICIT Version DEFAULT v1,
  if (!CBS_get_optional_asn1(
          &tbs_certificate, nullptr, nullptr,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  //        serialNumber         CertificateSerialNumber,
  if (!CBS_get_asn1(&tbs_certificate, nullptr, CBS_ASN1_INTEGER)) {
    return false;
  }
  //        signature            AlgorithmIdentifier
  if (!CBS_get_asn1(&tbs_certificate, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        issuer               Name,
  if (!CBS_get_asn1(&tbs_certificate, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        validity             Validity,
  CBS validity;
  if (!CBS_get_asn1(&tbs_certificate, &validity, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  // Skip over notBefore.
  if (!CBS_get_any_asn1_element(&validity, nullptr, nullptr, nullptr)) {
    return false;
  }
  // Parse notAfter.
  CBS not_after;
  unsigned not_after_tag;
  if (!CBS_get_any_asn1(&validity, &not_after, &not_after_tag)) {
    return false;
  }
  bool long_format;
  if (not_after_tag == CBS_ASN1_UTCTIME) {
    long_format = false;
  } else if (not_after_tag == CBS_ASN1_GENERALIZEDTIME) {
    long_format = true;
  } else {
    return false;
  }
  if (expiration_time) {
    *expiration_time =
        ASN1TimeToSec(CBS_data(&not_after), CBS_len(&not_after), long_format);
  }
  //        subject              Name,
  if (!CBS_get_asn1_element(&tbs_certificate, subject_name,
                            CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  if (!CBS_get_asn1(&tbs_certificate, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL
  if (!CBS_get_optional_asn1(&tbs_certificate, nullptr, nullptr,
                             0x01 | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL
  if (!CBS_get_optional_asn1(&tbs_certificate, nullptr, nullptr,
                             0x02 | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  // If extensions were not found, should return an empty CBS.
  if (extensions) {
    CBS_init(extensions, nullptr, 0);
  }
  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  if (!CBS_get_optional_asn1(
          &tbs_certificate, extensions, nullptr,
          0x03 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }

  return true;
}
#endif  // OPENSSL_IS_BORINGSSL

bool VerifyPeerCertMatchesHost(SSL* ssl, const std::string& host) {
  if (host.empty()) {
    RTC_DLOG(LS_ERROR) << "Hostname is empty. Cannot verify peer certificate.";
    return false;
  }

  if (ssl == nullptr) {
    RTC_DLOG(LS_ERROR) << "SSL is nullptr. Cannot verify peer certificate.";
    return false;
  }

#ifdef OPENSSL_IS_BORINGSSL
  const STACK_OF(CRYPTO_BUFFER)* chain = SSL_get0_peer_certificates(ssl);
  if (chain == nullptr || sk_CRYPTO_BUFFER_num(chain) == 0) {
    RTC_LOG(LS_ERROR)
        << "SSL_get0_peer_certificates failed. This should never happen.";
    return false;
  }
  CRYPTO_BUFFER* leaf = sk_CRYPTO_BUFFER_value(chain, 0);
  CBS subject_name, extensions;
  if (!ParseCertificate(leaf, nullptr, nullptr, &subject_name, &extensions)) {
    RTC_LOG(LS_ERROR) << "Failed to parse certificate.";
    return false;
  }

  // Implement name verification according to RFC6125.
  // First, look for the subject alt name extension and use it if available.
  if (CBS_len(&extensions)) {
    bool san_present = false;
    if (MatchSubjectAltName(&extensions, host, &san_present)) {
      return true;
    }
    if (san_present) {
      // Found subject alt names extension but didn't find any match. This
      // should be interpreted as a failure; should NOT use the subject name
      // for matching if the subject alt name exists.
      return false;
    }
  }

  // If no subject alt name extension was found, use the regular subject name.
  if (!CBS_len(&subject_name)) {
    return false;
  }
  return MatchSubjectName(&subject_name, host);
#else   // OPENSSL_IS_BORINGSSL
  X509* certificate = SSL_get_peer_certificate(ssl);
  if (certificate == nullptr) {
    RTC_LOG(LS_ERROR)
        << "SSL_get_peer_certificate failed. This should never happen.";
    return false;
  }

  LogCertificates(ssl, certificate);

  bool is_valid_cert_name =
      X509_check_host(certificate, host.c_str(), host.size(), 0, nullptr) == 1;
  X509_free(certificate);
  return is_valid_cert_name;
#endif  // !defined(OPENSSL_IS_BORINGSSL)
}

void LogSSLErrors(const std::string& prefix) {
  char error_buf[200];
  unsigned long err;  // NOLINT

  while ((err = ERR_get_error()) != 0) {
    ERR_error_string_n(err, error_buf, sizeof(error_buf));
    RTC_LOG(LS_ERROR) << prefix << ": " << error_buf << "\n";
  }
}

#ifndef WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS
bool LoadBuiltinSSLRootCertificates(SSL_CTX* ctx) {
  int count_of_added_certs = 0;
  for (size_t i = 0; i < arraysize(kSSLCertCertificateList); i++) {
    const unsigned char* cert_buffer = kSSLCertCertificateList[i];
    size_t cert_buffer_len = kSSLCertCertificateSizeList[i];
    X509* cert = d2i_X509(nullptr, &cert_buffer,
                          checked_cast<long>(cert_buffer_len));  // NOLINT
    if (cert) {
      int return_value = X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), cert);
      if (return_value == 0) {
        RTC_LOG(LS_WARNING) << "Unable to add certificate.";
      } else {
        count_of_added_certs++;
      }
      X509_free(cert);
    }
  }
  return count_of_added_certs > 0;
}
#endif  // WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS

#ifdef OPENSSL_IS_BORINGSSL
CRYPTO_BUFFER_POOL* GetBufferPool() {
  static CRYPTO_BUFFER_POOL* instance = CRYPTO_BUFFER_POOL_new();
  return instance;
}
#endif

}  // namespace openssl
}  // namespace rtc
