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
// restrictions used by BoringSSL;
bool WildcardMatch(const std::string& host, const std::string& pattern) {
  // "- 1" in case wildcard matches zero characters.
  if (pattern.size() > host.size() - 1) {
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
  // If the wildcard makes up the entire first label...
  if (host_prefix.empty() && host_suffix[0] == '.') {
    // ...it must match at least one character (e.g. "*.example.com" can't
    // match ".example.com"), ...
    if (host_wildcard_match.empty()) {
      return false;
    }
    // ... and MAY match a single *.
    if (host_wildcard_match == "*") {
      return true;
    }
  }
  // Wildcard can't match any '.'s (e.g. "*.example.com" can't match
  // "foo.bar.example.com")
  if (host_wildcard_match.find('.') != std::string::npos) {
    return false;
  }
  return absl::EqualsIgnoreCase(host_prefix, pattern_prefix) &&
         absl::EqualsIgnoreCase(host_suffix, pattern_suffix);
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
                      std::vector<uint8_t>* signature_algorithm_oid,
                      int64_t* expiration_time,
                      std::vector<uint8_t>* subject_name,
                      std::vector<uint8_t>* extensions) {
  CBS cbs;
  CRYPTO_BUFFER_init_CBS(cert_buffer, &cbs);

  //   Certificate  ::=  SEQUENCE  {
  CBS certificate_cbs;
  if (!CBS_get_asn1(&cbs, &certificate_cbs, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        tbsCertificate       TBSCertificate,
  CBS tbs_certificate_cbs;
  if (!CBS_get_asn1(&certificate_cbs, &tbs_certificate_cbs,
                    CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        signatureAlgorithm   AlgorithmIdentifier,
  CBS signature_algorithm_cbs;
  if (!CBS_get_asn1(&certificate_cbs, &signature_algorithm_cbs,
                    CBS_ASN1_SEQUENCE)) {
    return false;
  }
  CBS oid;
  if (!CBS_get_asn1(&cbs, &oid, CBS_ASN1_OBJECT)) {
    return false;
  }
  if (signature_algorithm_oid) {
    signature_algorithm_oid->assign(CBS_data(&oid),
                                    CBS_data(&oid) + CBS_len(&oid));
  }
  //        signatureValue       BIT STRING  }
  if (!CBS_get_asn1(&certificate_cbs, nullptr, CBS_ASN1_BITSTRING)) {
    return false;
  }
  // There isn't an extension point at the end of Certificate,an input should
  // have beena single certificate, so there should be no more unconsumed data.
  if (CBS_len(&certificate_cbs) || CBS_len(&cbs)) {
    return false;
  }

  // Now parse the inner TBSCertificate.
  //        version         [0]  EXPLICIT Version DEFAULT v1,
  if (!CBS_get_optional_asn1(
          &tbs_certificate_cbs, nullptr, nullptr,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  //        serialNumber         CertificateSerialNumber,
  if (!CBS_get_asn1(&tbs_certificate_cbs, nullptr, CBS_ASN1_INTEGER)) {
    return false;
  }
  //        signature            AlgorithmIdentifier
  if (!CBS_get_asn1(&tbs_certificate_cbs, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        issuer               Name,
  if (!CBS_get_asn1(&tbs_certificate_cbs, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        validity             Validity,
  CBS validity_cbs;
  if (!CBS_get_asn1(&tbs_certificate_cbs, &validity_cbs, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  // Skip over notBefore.
  if (!CBS_get_any_asn1(&validity_cbs, nullptr, nullptr)) {
    return false;
  }
  // Parse notAfter.
  CBS not_after_cbs;
  unsigned not_after_tag;
  if (!CBS_get_any_asn1(&validity_cbs, &not_after_cbs, &not_after_tag)) {
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
    *expiration_time = ASN1TimeToSec(CBS_data(&not_after_cbs),
                                     CBS_len(&not_after_cbs), long_format);
  }
  //        subject              Name,
  CBS subject_name_cbs;
  if (!CBS_get_asn1(&tbs_certificate_cbs, &subject_name_cbs,
                    CBS_ASN1_SEQUENCE)) {
    return false;
  }
  if (subject_name) {
    subject_name->assign(
        CBS_data(&subject_name_cbs),
        CBS_data(&subject_name_cbs) + CBS_len(&subject_name_cbs));
  }
  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  if (!CBS_get_asn1(&tbs_certificate_cbs, nullptr, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL
  if (!CBS_get_optional_asn1(&tbs_certificate_cbs, nullptr, nullptr,
                             0x01 | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL
  if (!CBS_get_optional_asn1(&tbs_certificate_cbs, nullptr, nullptr,
                             0x02 | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  CBS extensions_cbs;
  int extensions_present;
  if (!CBS_get_optional_asn1(
          &tbs_certificate_cbs, &extensions_cbs, &extensions_present,
          0x03 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return false;
  }
  if (extensions_present && extensions) {
    extensions->assign(CBS_data(&extensions_cbs),
                       CBS_data(&extensions_cbs) + CBS_len(&extensions_cbs));
  }
  // There shouldn't be any subsequent data in v1-3 certificates, which is all
  // this supports.
  if (CBS_len(&tbs_certificate_cbs)) {
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
  std::vector<uint8_t> subject_name, extensions;
  if (!ParseCertificate(leaf, nullptr, nullptr, &subject_name, &extensions)) {
    RTC_LOG(LS_ERROR) << "Failed to parse certificate.";
    return false;
  }

  // Implement name verification according to RFC6125.
  // First, look for the subject alt name extension and use it if available.
  if (!extensions.empty()) {
    CBS extensions_cbs;
    CBS_init(&extensions_cbs, &extensions[0], extensions.size());
    //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
    CBS extension_sequence_cbs;
    if (!CBS_get_asn1(&extensions_cbs, &extension_sequence_cbs,
                      CBS_ASN1_SEQUENCE)) {
      return false;
    }
    // Extensions should be a single sequence.
    if (CBS_len(&extensions_cbs)) {
      return false;
    }
    while (CBS_len(&extension_sequence_cbs)) {
      //    Extension  ::=  SEQUENCE  {
      CBS extension_cbs;
      if (!CBS_get_asn1(&extension_sequence_cbs, &extension_cbs,
                        CBS_ASN1_SEQUENCE)) {
        return false;
      }
      //            extnID      OBJECT IDENTIFIER,
      CBS extension_oid;
      if (!CBS_get_asn1(&extension_cbs, &extension_oid, CBS_ASN1_OBJECT)) {
        return false;
      }
      // We're only interested in the subject alt name extension.
      static const uint8_t kSubjectAltName[] = {0x55, 0x1d, 0x11};
      if (CBS_len(&extension_oid) != sizeof(kSubjectAltName) ||
          0 != memcmp(kSubjectAltName, CBS_data(&extension_oid),
                      sizeof(kSubjectAltName))) {
        continue;
      }
      //            critical    BOOLEAN DEFAULT FALSE,
      if (!CBS_get_optional_asn1(&extension_cbs, nullptr, nullptr,
                                 CBS_ASN1_BOOLEAN)) {
        return false;
      }
      //            extnValue   OCTET STRING
      CBS extension_value_cbs;
      if (!CBS_get_asn1(&extension_cbs, &extension_value_cbs,
                        CBS_ASN1_OCTETSTRING)) {
        return false;
      }
      // There should be nothing after extnValue.
      if (CBS_len(&extension_cbs)) {
        return false;
      }
      // RFC 5280 section 4.2.1.6:
      // GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
      CBS general_names_cbs;
      if (!CBS_get_asn1(&extension_value_cbs, &general_names_cbs,
                        CBS_ASN1_SEQUENCE)) {
        return false;
      }
      // There should be nothing after GeneralNames.
      if (CBS_len(&extension_value_cbs)) {
        return false;
      }
      while (CBS_len(&general_names_cbs)) {
        CBS general_name_cbs;
        unsigned general_name_tag;
        if (!CBS_get_any_asn1(&general_names_cbs, &general_name_cbs,
                              &general_name_tag)) {
          return false;
        }
        // Only interested in DNS name.
        // dNSName                         [2]     IA5String,
        if (general_name_tag != (0x2 | CBS_ASN1_CONTEXT_SPECIFIC)) {
          continue;
        }
        if (WildcardMatch(host, std::string(CBS_data(&general_name_cbs),
                                            CBS_data(&general_name_cbs) +
                                                CBS_len(&general_name_cbs)))) {
          return true;
        }
      }
      // Found subject alt names extension but didn't find any match.
      return false;
    }  // while (CBS_len(&extension_sequence_cbs))
  }    // if (!extensions.empty())

  // If no subject alt name extension was found, use the common name.
  CBS subject_name_cbs;
  CBS_init(&subject_name_cbs, &subject_name[0], subject_name.size());
  // RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
  CBS rdn_sequence_cbs;
  if (!CBS_get_asn1(&subject_name_cbs, &rdn_sequence_cbs, CBS_ASN1_SEQUENCE)) {
    return false;
  }
  while (CBS_len(&rdn_sequence_cbs)) {
    // RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue
    CBS rdn_cbs;
    if (!CBS_get_asn1(&rdn_sequence_cbs, &rdn_cbs, CBS_ASN1_SET)) {
      return false;
    }
    while (CBS_len(&rdn_cbs)) {
      // AttributeTypeAndValue ::= SEQUENCE {
      CBS type_and_value_cbs;
      if (!CBS_get_asn1(&rdn_cbs, &type_and_value_cbs, CBS_ASN1_SEQUENCE)) {
        return false;
      }
      // AttributeType ::= OBJECT IDENTIFIER
      CBS type_cbs;
      if (!CBS_get_asn1(&type_and_value_cbs, &type_cbs, CBS_ASN1_OBJECT)) {
        return false;
      }
      // We're only interested in the common name.
      static const uint8_t kCommonName[] = {0x55, 0x04, 0x03};
      if (CBS_len(&type_cbs) != sizeof(kCommonName) ||
          0 != memcmp(kCommonName, CBS_data(&type_cbs), sizeof(kCommonName))) {
        continue;
      }
      // AttributeValue ::= ANY -- DEFINED BY AttributeType
      unsigned common_name_tag;
      CBS common_name_cbs;
      if (!CBS_get_any_asn1(&type_and_value_cbs, &common_name_cbs,
                            &common_name_tag)) {
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
    }  // while (CBS_len(&rdn_cbs))
  }    // while (CBS_len(&rdn_sequence_cbs))
  // No subject alt name or common name found that matches.
  return false;
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
