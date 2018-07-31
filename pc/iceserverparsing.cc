/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/iceserverparsing.h"

#include <cctype>  // For std::isdigit.
#include <string>

#include "rtc_base/arraysize.h"

namespace webrtc {

// The min number of tokens must present in Turn host uri.
// e.g. user@turn.example.org
static const size_t kTurnHostTokensNum = 2;
// Number of tokens must be preset when TURN uri has transport param.
static const size_t kTurnTransportTokensNum = 2;
// The default stun port.
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTransport[] = "transport";

// NOTE: Must be in the same order as the ServiceType enum.
static const char* kValidIceServiceTypes[] = {"stun", "stuns", "turn", "turns"};

// NOTE: A loop below assumes that the first value of this enum is 0 and all
// other values are incremental.
enum ServiceType {
  STUN = 0,  // Indicates a STUN server.
  STUNS,     // Indicates a STUN server used with a TLS session.
  TURN,      // Indicates a TURN server
  TURNS,     // Indicates a TURN server used with a TLS session.
  INVALID,   // Unknown.
};
static_assert(INVALID == arraysize(kValidIceServiceTypes),
              "kValidIceServiceTypes must have as many strings as ServiceType "
              "has values.");

// |in_str| should be of format
// stunURI       = scheme ":" stun-host [ ":" stun-port ]
// scheme        = "stun" / "stuns"
// stun-host     = IP-literal / IPv4address / reg-name
// stun-port     = *DIGIT
//
// draft-petithuguenin-behave-turn-uris-01
// turnURI       = scheme ":" turn-host [ ":" turn-port ]
// turn-host     = username@IP-literal / IPv4address / reg-name
static RTCError GetServiceTypeAndHostnameFromUri(const std::string& in_str,
                                                 ServiceType* service_type,
                                                 std::string* hostname) {
  const std::string::size_type colonpos = in_str.find(':');
  if (colonpos == std::string::npos) {
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                            "Missing ':' in ICE URI.", LS_WARNING);
  }
  if ((colonpos + 1) == in_str.length()) {
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                            "Empty hostname in ICE URI.", LS_WARNING);
  }
  *service_type = INVALID;
  for (size_t i = 0; i < arraysize(kValidIceServiceTypes); ++i) {
    if (in_str.compare(0, colonpos, kValidIceServiceTypes[i]) == 0) {
      *service_type = static_cast<ServiceType>(i);
      break;
    }
  }
  if (*service_type == INVALID) {
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                            "Invalid service type in ICE URI.", LS_WARNING);
  }
  *hostname = in_str.substr(colonpos + 1, std::string::npos);
  return RTCError::OK();
}

static bool ParsePort(const std::string& in_str, int* port) {
  // Make sure port only contains digits. FromString doesn't check this.
  for (const char& c : in_str) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return rtc::FromString(in_str, port);
}

// This method parses IPv6 and IPv4 literal strings, along with hostnames in
// standard hostname:port format.
// Consider following formats as correct.
// |hostname:port|, |[IPV6 address]:port|, |IPv4 address|:port,
// |hostname|, |[IPv6 address]|, |IPv4 address|.
static bool ParseHostnameAndPortFromString(const std::string& in_str,
                                           std::string* host,
                                           int* port) {
  RTC_DCHECK(host->empty());
  if (in_str.at(0) == '[') {
    std::string::size_type closebracket = in_str.rfind(']');
    if (closebracket != std::string::npos) {
      std::string::size_type colonpos = in_str.find(':', closebracket);
      if (std::string::npos != colonpos) {
        if (!ParsePort(in_str.substr(closebracket + 2, std::string::npos),
                       port)) {
          return false;
        }
      }
      *host = in_str.substr(1, closebracket - 1);
    } else {
      return false;
    }
  } else {
    std::string::size_type colonpos = in_str.find(':');
    if (std::string::npos != colonpos) {
      if (!ParsePort(in_str.substr(colonpos + 1, std::string::npos), port)) {
        return false;
      }
      *host = in_str.substr(0, colonpos);
    } else {
      *host = in_str;
    }
  }
  return !host->empty();
}

// Adds a STUN or TURN server to the appropriate list,
// by parsing |url| and using the username/password in |server|.
static RTCError ParseIceServerUrl(
    const PeerConnectionInterface::IceServer& server,
    const std::string& url,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers) {
  // draft-nandakumar-rtcweb-stun-uri-01
  // stunURI       = scheme ":" stun-host [ ":" stun-port ]
  // scheme        = "stun" / "stuns"
  // stun-host     = IP-literal / IPv4address / reg-name
  // stun-port     = *DIGIT

  // draft-petithuguenin-behave-turn-uris-01
  // turnURI       = scheme ":" turn-host [ ":" turn-port ]
  //                 [ "?transport=" transport ]
  // scheme        = "turn" / "turns"
  // transport     = "udp" / "tcp" / transport-ext
  // transport-ext = 1*unreserved
  // turn-host     = IP-literal / IPv4address / reg-name
  // turn-port     = *DIGIT
  RTC_DCHECK(stun_servers);
  RTC_DCHECK(turn_servers);
  RTC_DCHECK(!url.empty());
  std::vector<std::string> tokens;
  cricket::ProtocolType turn_transport_type = cricket::PROTO_UDP;
  rtc::tokenize_with_empty_tokens(url, '?', &tokens);
  std::string uri_without_transport = tokens[0];

  // Let's look into transport= param, if it exists.
  if (tokens.size() == kTurnTransportTokensNum) {  // ?transport= is present.
    std::string uri_transport_param = tokens[1];
    rtc::tokenize_with_empty_tokens(uri_transport_param, '=', &tokens);
    if (tokens[0] != kTransport) {
      LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                              "Invalid transport parameter key.", LS_WARNING);
    }
    if (tokens.size() < 2) {
      LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                              "Transport parameter missing value.", LS_WARNING);
    }
    if (!cricket::StringToProto(tokens[1].c_str(), &turn_transport_type) ||
        (turn_transport_type != cricket::PROTO_UDP &&
         turn_transport_type != cricket::PROTO_TCP)) {
      LOG_AND_RETURN_ERROR_EX(
          RTCErrorType::SYNTAX_ERROR,
          "Transport parameter should always be udp or tcp.", LS_WARNING);
    }
  }

  std::string hoststring;
  ServiceType service_type;
  RTCError error = GetServiceTypeAndHostnameFromUri(uri_without_transport,
                                                    &service_type, &hoststring);
  if (!error.ok()) {
    return error;
  }

  // GetServiceTypeAndHostnameFromUri should never give an empty hoststring
  RTC_DCHECK(!hoststring.empty());

  // Let's break hostname.
  tokens.clear();
  rtc::tokenize_with_empty_tokens(hoststring, '@', &tokens);

  std::string username(server.username);
  if (tokens.size() > kTurnHostTokensNum) {
    char buf[1024];
    rtc::SimpleStringBuilder sb(buf);
    sb << "Invalid user@hostname format: " << hoststring;
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR, sb.str(), LS_WARNING);
  }
  if (tokens.size() == kTurnHostTokensNum) {
    if (tokens[0].empty() || tokens[1].empty()) {
      char buf[1024];
      rtc::SimpleStringBuilder sb(buf);
      sb << "Invalid user@hostname format: " << hoststring;
      LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR, sb.str(), LS_WARNING);
    }
    username.assign(rtc::s_url_decode(tokens[0]));
    hoststring = tokens[1];
  } else {
    hoststring = tokens[0];
  }

  int port = kDefaultStunPort;
  if (service_type == TURNS) {
    port = kDefaultStunTlsPort;
    turn_transport_type = cricket::PROTO_TLS;
  }

  std::string address;
  if (!ParseHostnameAndPortFromString(hoststring, &address, &port)) {
    char buf[1024];
    rtc::SimpleStringBuilder sb(buf);
    sb << "Invalid hostname format: " << uri_without_transport;
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR, sb.str(), LS_WARNING);
  }

  if (port <= 0 || port > 0xffff) {
    char buf[1024];
    rtc::SimpleStringBuilder sb(buf);
    sb << "Invalid port: " << port;
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR, sb.str(), LS_WARNING);
  }

  switch (service_type) {
    case STUN:
    case STUNS:
      stun_servers->insert(rtc::SocketAddress(address, port));
      break;
    case TURN:
    case TURNS: {
      if (username.empty() || server.password.empty()) {
        // The WebRTC spec requires throwing an InvalidAccessError when username
        // or credential are ommitted; this is the native equivalent.
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "TURN URL without username, or password empty.");
      }
      // If the hostname field is not empty, then the server address must be
      // the resolved IP for that host, the hostname is needed later for TLS
      // handshake (SNI and Certificate verification).
      const std::string& hostname =
          server.hostname.empty() ? address : server.hostname;
      rtc::SocketAddress socket_address(hostname, port);
      if (!server.hostname.empty()) {
        rtc::IPAddress ip;
        if (!IPFromString(address, &ip)) {
          // When hostname is set, the server address must be a
          // resolved ip address.
          LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                               "IceServer has hostname field set, but URI does "
                               "not contain an IP address.");
        }
        socket_address.SetResolvedIP(ip);
      }
      cricket::RelayServerConfig config = cricket::RelayServerConfig(
          socket_address, username, server.password, turn_transport_type);
      if (server.tls_cert_policy ==
          PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck) {
        config.tls_cert_policy =
            cricket::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK;
      }
      config.tls_alpn_protocols = server.tls_alpn_protocols;
      config.tls_elliptic_curves = server.tls_elliptic_curves;

      turn_servers->push_back(config);
      break;
    }
    default:
      // We shouldn't get to this point with an invalid service_type, we should
      // have returned an error already.
      RTC_NOTREACHED();
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Unexpected service type.");
  }
  return RTCError::OK();
}

static std::vector<std::string> GetIceServerUrls(
    const PeerConnectionInterface::IceServer& server) {
  // Prefer |urls| over |uri| if present.
  if (!server.urls.empty()) {
    return server.urls;
  }
  if (!server.uri.empty()) {
    return {server.uri};
  }
  return {};
}

static RTCError ParseIceServer(
    const PeerConnectionInterface::IceServer& server,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers) {
  std::vector<std::string> urls = GetIceServerUrls(server);
  if (urls.empty()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::SYNTAX_ERROR,
                         "Failed to parse ICE server: No URL given.");
  }
  for (const std::string& url : GetIceServerUrls(server)) {
    if (url.empty()) {
      LOG_AND_RETURN_ERROR_EX(RTCErrorType::SYNTAX_ERROR,
                              "Failed to parse ICE server: URL is empty.",
                              LS_WARNING);
    }
    RTCError error = ParseIceServerUrl(server, url, stun_servers, turn_servers);
    if (!error.ok()) {
      char buf[1024];
      rtc::SimpleStringBuilder sb(buf);
      sb << "Failed to parse ICE server (with URL '" << url
         << "'): " << error.message();
      LOG_AND_RETURN_ERROR_EX(error.type(), sb.str(), LS_WARNING);
    }
  }
  return RTCError::OK();
}

RTCError ParseIceServers(
    const PeerConnectionInterface::IceServers& servers,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers) {
  for (size_t i = 0; i < servers.size(); ++i) {
    const PeerConnectionInterface::IceServer& server = servers[i];
    RTCError error = ParseIceServer(server, stun_servers, turn_servers);
    if (!error.ok()) {
      char buf[1024];
      rtc::SimpleStringBuilder sb(buf);
      sb << "[at index=" << i << "] " << error.message();
      LOG_AND_RETURN_ERROR_EX(error.type(), sb.str(), LS_WARNING);
    }
  }
  // Candidates must have unique priorities, so that connectivity checks
  // are performed in a well-defined order.
  int priority = static_cast<int>(turn_servers->size() - 1);
  for (cricket::RelayServerConfig& turn_server : *turn_servers) {
    // First in the list gets highest priority.
    turn_server.priority = priority--;
  }
  return RTCError::OK();
}

}  // namespace webrtc
