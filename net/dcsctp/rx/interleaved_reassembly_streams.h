/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_RX_INTERLEAVED_REASSEMBLY_STREAMS_H_
#define NET_DCSCTP_RX_INTERLEAVED_REASSEMBLY_STREAMS_H_

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/rx/reassembly_streams.h"

namespace dcsctp {

class InterleavedReassemblyStreams : public ReassemblyStreams {
 public:
  InterleavedReassemblyStreams(absl::string_view log_prefix,
                               OnAssembledMessage on_assembled_message)
      : log_prefix_(log_prefix), on_assembled_message_(on_assembled_message) {}

  int Add(UnwrappedTSN tsn, Data data) override;

  size_t HandleForwardTsn(
      UnwrappedTSN new_cumulative_ack_tsn,
      rtc::ArrayView<const AnyForwardTsnChunk::SkippedStream> skipped_streams)
      override;

  void ResetStreams(rtc::ArrayView<const StreamID> stream_ids) override;

 private:
  using ChunkMap = std::map<FSN, std::pair<UnwrappedTSN, Data>>;

  class StreamBase {
   protected:
    explicit StreamBase(InterleavedReassemblyStreams* parent)
        : parent_(*parent) {}

    size_t TryToAssembleMessage(UnwrappedMID mid);
    size_t AssembleMessage(const ChunkMap& tsn_chunks);

    InterleavedReassemblyStreams& parent_;
    std::map<UnwrappedMID, ChunkMap> chunks_by_mid_;
    UnwrappedMID::Unwrapper mid_unwrapper_;
  };

  class UnorderedStream : public StreamBase {
   public:
    explicit UnorderedStream(InterleavedReassemblyStreams* parent)
        : StreamBase(parent) {}
    int Add(UnwrappedTSN tsn, Data data);
    size_t EraseTo(MID message_id);
    void Reset() { mid_unwrapper_.Reset(); }
  };

  class OrderedStream : public StreamBase {
   public:
    explicit OrderedStream(InterleavedReassemblyStreams* parent)
        : StreamBase(parent), next_mid_(mid_unwrapper_.Unwrap(MID(0))) {}
    int Add(UnwrappedTSN tsn, Data data);
    size_t EraseTo(MID message_id);
    size_t TryToAssembleMessages();

    void Reset() {
      mid_unwrapper_.Reset();
      next_mid_ = mid_unwrapper_.Unwrap(MID(0));
    }

   private:
    UnwrappedMID next_mid_;
  };

  const std::string log_prefix_;
  OnAssembledMessage on_assembled_message_;
  std::unordered_map<StreamID, UnorderedStream, StreamID::Hasher>
      unordered_chunks_;
  std::unordered_map<StreamID, OrderedStream, StreamID::Hasher> ordered_chunks_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_RX_INTERLEAVED_REASSEMBLY_STREAMS_H_
