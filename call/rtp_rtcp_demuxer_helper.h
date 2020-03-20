/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_RTCP_DEMUXER_HELPER_H_
#define CALL_RTP_RTCP_DEMUXER_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {

// TODO(eladalon): Remove this in the next CL.
template <typename Container>
bool MultimapAssociationExists(const Container& multimap,
                               const typename Container::key_type& key,
                               const typename Container::mapped_type& val) {
  auto it_range = multimap.equal_range(key);
  using Reference = typename Container::const_reference;
  return std::any_of(it_range.first, it_range.second,
                     [val](Reference elem) { return elem.second == val; });
}

template <typename Container, typename Value>
size_t RemoveFromMultimapByValue(Container* multimap, const Value& value) {
  size_t count = 0;
  for (auto it = multimap->begin(); it != multimap->end();) {
    if (it->second == value) {
      it = multimap->erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

template <typename Map, typename Value>
size_t RemoveFromMapByValue(Map* map, const Value& value) {
  size_t count = 0;
  for (auto it = map->begin(); it != map->end();) {
    if (it->second == value) {
      it = map->erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

template <typename Key, typename Value, typename ComparisonValue>
size_t RemoveFromAbslHashMapByValue(absl::flat_hash_map<Key, Value>* map,
                                    const ComparisonValue& value) {
  size_t count = 0;
  for (auto it = map->begin(); it != map->end();) {
    if (it->second == value) {
      // https://abseil.io/docs/cpp/guides/container#abslflat_hash_map-and-abslflat_hash_set
      //
      // absl::flat_hash_map and absl::flat_hash_set: Iterators, references, and
      // pointers to elements are invalidated on rehash. absl::node_hash_map and
      // absl::node_hash_set: Iterators are invalidated on rehash.
      //
      // "Erasing does not trigger a rehash."
      // https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/container/flat_hash_map.h?l=215
      //
      // "If that iterator is needed, simply post increment the iterator"
      // https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/container/flat_hash_map.h?g=0&l=226
      //
      // "The iterator is invalidated, so any increment should be done before
      // calling erase."
      // https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/container/internal/raw_hash_set.h?type=cs&q=raw_hash_set&g=0&l=1157
      map->erase(it++);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

template <typename Container, typename Key>
bool ContainerHasKey(const Container& c, const Key& k) {
  return std::find(c.cbegin(), c.cend(), k) != c.cend();
}

// TODO(eladalon): Remove this in the next CL.
template <typename Container>
bool MultimapHasValue(const Container& c,
                      const typename Container::mapped_type& v) {
  auto predicate = [v](const typename Container::value_type& it) {
    return it.second == v;
  };
  return std::any_of(c.cbegin(), c.cend(), predicate);
}

template <typename Map>
bool MapHasValue(const Map& map, const typename Map::mapped_type& value) {
  auto predicate = [value](const typename Map::value_type& it) {
    return it.second == value;
  };
  return std::any_of(map.cbegin(), map.cend(), predicate);
}

template <typename Container>
bool MultimapHasKey(const Container& c,
                    const typename Container::key_type& key) {
  auto it_range = c.equal_range(key);
  return it_range.first != it_range.second;
}

absl::optional<uint32_t> ParseRtcpPacketSenderSsrc(
    rtc::ArrayView<const uint8_t> packet);

}  // namespace webrtc

#endif  // CALL_RTP_RTCP_DEMUXER_HELPER_H_
