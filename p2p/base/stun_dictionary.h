/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_STUN_DICTIONARY_H_
#define P2P_BASE_STUN_DICTIONARY_H_

#include <deque>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/rtc_error.h"
#include "api/transport/stun.h"

namespace cricket {

// StunDictionaryReader is a dictionary of StunAttributes.
//
// The StunAttributes can be read using the |Get|-methods.
//
// The content of the dictionary is updated by using the |ApplyDelta|-method.
// The |update| argument to the |ApplyDelta|-method is created by
// |StunDictionaryWriter::CreateDelta|-method.
class StunDictionaryReader {
 public:
  // A reserved key used to transport the version number
  static constexpr uint16_t kVersionKey = 0xFFFF;

  // A magic number used when transporting deltas.
  static constexpr uint16_t kDeltaMagic = 0x7788;

  // The version number for the delta format.
  static constexpr uint16_t kDeltaVersion = 0x1;

  StunDictionaryReader();

  // Gets the desired attribute value, or NULL if no such attribute type exists.
  const StunAddressAttribute* GetAddress(int key) const;
  const StunUInt32Attribute* GetUInt32(int key) const;
  const StunUInt64Attribute* GetUInt64(int key) const;
  const StunByteStringAttribute* GetByteString(int key) const;
  const StunUInt16ListAttribute* GetUInt16List(int key) const;

  bool empty() const { return attrs_.empty(); }
  int size() const { return attrs_.size(); }
  int bytes_stored() const { return bytes_stored_; }
  void set_max_bytes_stored(int max_bytes_stored) {
    max_bytes_stored_ = max_bytes_stored;
  }

  // Apply a delta and return
  // a pair with
  // - StunUInt64Attribute to ack the update.
  // - vector or keys that was modified.
  webrtc::RTCErrorOr<
      std::pair<std::unique_ptr<StunUInt64Attribute>, std::vector<uint16_t>>>
  ApplyDelta(const StunByteStringAttribute& update);

  // Make an exact copy of a StunDictionaryReader.
  // This should only be used for testing.
  StunDictionaryReader* Clone() const;

 protected:
  const StunAttribute* GetOrNull(int key) const;
  size_t GetLength(int key) const;
  static webrtc::RTCErrorOr<
      std::pair<uint64_t, std::deque<std::unique_ptr<StunAttribute>>>>
  ParseDelta(const StunByteStringAttribute& update);

  // content of dictionary
  int64_t version_ = 1;
  std::map<uint16_t, std::unique_ptr<StunAttribute>> attrs_;
  std::map<uint16_t, uint64_t> version_per_key_;

  int max_bytes_stored_ = 16384;
  int bytes_stored_ = 0;
};

class StunDictionaryWriter : public StunDictionaryReader {
 public:
  StunDictionaryWriter();

  // Record a modification to |key|
  // and return an StunAttribute that can be modified.
  StunAddressAttribute* SetAddress(int key);
  StunUInt32Attribute* SetUInt32(int key);
  StunUInt64Attribute* SetUInt64(int key);
  StunByteStringAttribute* SetByteString(int key);
  StunUInt16ListAttribute* SetUInt16List(int key);

  // Delete a key.
  bool Delete(int key);

  // Check if a key has a pending change (i.e a change
  // that has not been acked).
  bool Pending(int key) const;

  // Return number of of pending modifications.
  int Pending() const;

  // Create an StunByteStringAttribute containing the pending (e.g not ack:ed)
  // modifications.
  std::unique_ptr<StunByteStringAttribute> CreateDelta();

  // Apply an delta ack.
  void ApplyDeltaAck(const StunUInt64Attribute&);

  // Make an exact copy of a StunDictionaryWriter.
  // This should only be used for testing.
  StunDictionaryWriter* Clone() const;

 private:
  void Set(std::unique_ptr<StunAttribute> attr);

  // sorted list of changes that has not been yet been ack:ed.
  std::vector<std::pair<uint64_t, const StunAttribute*>> pending_;

  // tombstones, i.e values that has been deleted but not yet acked.
  std::map<uint16_t, std::unique_ptr<StunAttribute>> tombstones_;
};

}  // namespace cricket

#endif  // P2P_BASE_STUN_DICTIONARY_H_
