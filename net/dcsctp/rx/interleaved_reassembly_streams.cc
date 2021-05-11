/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/interleaved_reassembly_streams.h"

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "rtc_base/logging.h"

namespace dcsctp {
namespace {
size_t EraseMessagesTo(
    std::map<UnwrappedMID, std::map<FSN, std::pair<UnwrappedTSN, Data>>>&
        chunk_map,
    UnwrappedMID mid) {
  size_t removed_bytes = 0;
  auto it = chunk_map.begin();
  while (it != chunk_map.end() && it->first <= mid) {
    removed_bytes += absl::c_accumulate(
        it->second, 0,
        [](size_t r2, const auto& q) { return r2 + q.second.second.size(); });
    it = chunk_map.erase(it);
  }
  return removed_bytes;
}
}  // namespace

InterleavedReassemblyStreams::InterleavedReassemblyStreams(
    absl::string_view log_prefix,
    OnAssembledMessage on_assembled_message,
    const DcSctpSocketHandoverState* handover_state)
    : log_prefix_(log_prefix), on_assembled_message_(on_assembled_message) {
  if (handover_state) {
    for (const DcSctpSocketHandoverState::OrderedStream& state_stream :
         handover_state->rx.ordered_streams) {
      ordered_streams_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(StreamID(state_stream.id)),
          std::forward_as_tuple(this, MID(state_stream.next_ssn)));
    }
    for (const DcSctpSocketHandoverState::UnorderedStream& state_stream :
         handover_state->rx.unordered_streams) {
      unordered_streams_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(StreamID(state_stream.id)),
          std::forward_as_tuple(this));
    }
  }
}

size_t InterleavedReassemblyStreams::StreamBase::TryToAssembleMessage(
    UnwrappedMID mid) {
  std::map<UnwrappedMID, ChunkMap>::const_iterator it =
      chunks_by_mid_.find(mid);
  if (it == chunks_by_mid_.end()) {
    RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "TryToAssembleMessage "
                         << *mid.Wrap() << " - no chunks";
    return 0;
  }
  const ChunkMap& chunks = it->second;
  if (!chunks.begin()->second.second.is_beginning ||
      !chunks.rbegin()->second.second.is_end) {
    RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "TryToAssembleMessage "
                         << *mid.Wrap() << "- missing beginning or end";
    return 0;
  }
  int64_t fsn_diff = *chunks.rbegin()->first - *chunks.begin()->first;
  if (fsn_diff != (static_cast<ssize_t>(chunks.size()) - 1)) {
    RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "TryToAssembleMessage "
                         << *mid.Wrap() << "- not all chunks exist (have "
                         << chunks.size() << ", expect " << (fsn_diff + 1)
                         << ")";
    return 0;
  }

  size_t removed_bytes = AssembleMessage(chunks);
  RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "TryToAssembleMessage "
                       << *mid.Wrap() << " - succeeded and removed "
                       << removed_bytes;

  chunks_by_mid_.erase(mid);
  return removed_bytes;
}

size_t InterleavedReassemblyStreams::StreamBase::AssembleMessage(
    const ChunkMap& tsn_chunks) {
  size_t count = tsn_chunks.size();
  if (count == 1) {
    // Fast path - zero-copy
    const Data& data = tsn_chunks.begin()->second.second;
    size_t payload_size = data.size();
    UnwrappedTSN tsns[1] = {tsn_chunks.begin()->second.first};
    DcSctpMessage message(data.stream_id, data.ppid, std::move(data.payload));
    parent_.on_assembled_message_(tsns, std::move(message));
    return payload_size;
  }

  // Slow path - will need to concatenate the payload.
  std::vector<UnwrappedTSN> tsns;
  tsns.reserve(count);

  std::vector<uint8_t> payload;
  size_t payload_size = absl::c_accumulate(
      tsn_chunks, 0,
      [](size_t v, const auto& p) { return v + p.second.second.size(); });
  payload.reserve(payload_size);

  for (auto& item : tsn_chunks) {
    const UnwrappedTSN tsn = item.second.first;
    const Data& data = item.second.second;
    tsns.push_back(tsn);
    payload.insert(payload.end(), data.payload.begin(), data.payload.end());
  }

  const Data& data = tsn_chunks.begin()->second.second;

  DcSctpMessage message(data.stream_id, data.ppid, std::move(payload));
  parent_.on_assembled_message_(tsns, std::move(message));
  return payload_size;
}

size_t InterleavedReassemblyStreams::UnorderedStream::EraseTo(MID message_id) {
  UnwrappedMID unwrapped_mid = mid_unwrapper_.Unwrap(message_id);

  return EraseMessagesTo(chunks_by_mid_, unwrapped_mid);
}

size_t InterleavedReassemblyStreams::OrderedStream::EraseTo(MID message_id) {
  UnwrappedMID unwrapped_mid = mid_unwrapper_.Unwrap(message_id);

  size_t removed_bytes = EraseMessagesTo(chunks_by_mid_, unwrapped_mid);

  if (unwrapped_mid >= next_mid_) {
    RTC_DLOG(LS_VERBOSE) << "Erasing messages to " << *unwrapped_mid.Wrap()
                         << ", later than prev next expected "
                         << *next_mid_.Wrap();
    next_mid_ = unwrapped_mid.next_value();
  }

  removed_bytes += TryToAssembleMessages();

  return removed_bytes;
}

int InterleavedReassemblyStreams::UnorderedStream::Add(UnwrappedTSN tsn,
                                                       Data data) {
  int queued_bytes = data.size();
  UnwrappedMID mid = mid_unwrapper_.Unwrap(data.message_id);
  FSN fsn = data.fsn;
  auto [unused, inserted] =
      chunks_by_mid_[mid].emplace(fsn, std::make_pair(tsn, std::move(data)));
  if (!inserted) {
    return 0;
  }

  queued_bytes -= TryToAssembleMessage(mid);

  return queued_bytes;
}

int InterleavedReassemblyStreams::OrderedStream::Add(UnwrappedTSN tsn,
                                                     Data data) {
  int queued_bytes = data.size();
  UnwrappedMID mid = mid_unwrapper_.Unwrap(data.message_id);
  FSN fsn = data.fsn;
  auto [unused, inserted] =
      chunks_by_mid_[mid].emplace(fsn, std::make_pair(tsn, std::move(data)));
  if (!inserted) {
    return 0;
  }

  if (mid == next_mid_) {
    queued_bytes -= TryToAssembleMessages();
  }

  return queued_bytes;
}

size_t InterleavedReassemblyStreams::OrderedStream::TryToAssembleMessages() {
  size_t removed_bytes = 0;

  for (;;) {
    size_t removed_bytes_this_iter = TryToAssembleMessage(next_mid_);
    if (removed_bytes_this_iter == 0) {
      break;
    }

    removed_bytes += removed_bytes_this_iter;
    next_mid_.Increment();
  }
  return removed_bytes;
}

int InterleavedReassemblyStreams::Add(UnwrappedTSN tsn, Data data) {
  if (data.is_unordered) {
    auto it = unordered_streams_.emplace(data.stream_id, this).first;
    return it->second.Add(tsn, std::move(data));
  }

  auto it = ordered_streams_.emplace(data.stream_id, this).first;
  return it->second.Add(tsn, std::move(data));
}

size_t InterleavedReassemblyStreams::HandleForwardTsn(
    UnwrappedTSN new_cumulative_ack_tsn,
    rtc::ArrayView<const AnyForwardTsnChunk::SkippedStream> skipped_streams) {
  size_t removed_bytes = 0;
  for (const auto& skipped_stream : skipped_streams) {
    if (skipped_stream.unordered) {
      auto it = unordered_streams_.find(skipped_stream.stream_id);
      if (it != unordered_streams_.end()) {
        removed_bytes += it->second.EraseTo(skipped_stream.message_id);
      } else {
        RTC_DLOG(LS_VERBOSE)
            << log_prefix_ << "I-FORWARD-TSN reference unordered stream "
            << *skipped_stream.stream_id << " that does not exist";
      }
    } else {
      auto it = ordered_streams_.find(skipped_stream.stream_id);
      if (it != ordered_streams_.end()) {
        removed_bytes += it->second.EraseTo(skipped_stream.message_id);
      } else {
        RTC_DLOG(LS_VERBOSE)
            << log_prefix_ << "I-FORWARD-TSN reference ordered stream "
            << *skipped_stream.stream_id << " that does not exist";
      }
    }
  }
  return removed_bytes;
}

void InterleavedReassemblyStreams::ResetStreams(
    rtc::ArrayView<const StreamID> stream_ids) {
  if (stream_ids.empty()) {
    for (auto& entry : ordered_streams_) {
      entry.second.Reset();
    }
    for (auto& entry : unordered_streams_) {
      entry.second.Reset();
    }
  } else {
    for (StreamID stream_id : stream_ids) {
      auto ordered_it = ordered_streams_.find(stream_id);
      if (ordered_it != ordered_streams_.end()) {
        ordered_it->second.Reset();
      }
      auto unordered_it = unordered_streams_.find(stream_id);
      if (unordered_it != unordered_streams_.end()) {
        unordered_it->second.Reset();
      }
    }
  }
}

HandoverReadinessStatus InterleavedReassemblyStreams::GetHandoverReadiness()
    const {
  HandoverReadinessStatus status;
  for (const auto& [unused, stream] : ordered_streams_) {
    if (stream.has_unassembled_chunks()) {
      status.Add(HandoverUnreadinessReason::kOrderedStreamHasUnassembledChunks);
      break;
    }
  }
  for (const auto& [unused, stream] : unordered_streams_) {
    if (stream.has_unassembled_chunks()) {
      status.Add(
          HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks);
      break;
    }
  }
  return status;
}

void InterleavedReassemblyStreams::AddHandoverState(
    DcSctpSocketHandoverState& state) {
  for (const auto& [stream_id, stream] : ordered_streams_) {
    DcSctpSocketHandoverState::OrderedStream state_stream;
    state_stream.id = stream_id.value();
    state_stream.next_ssn = stream.next_mid().value();
    state.rx.ordered_streams.push_back(std::move(state_stream));
  }
  for (const auto& [stream_id, unused] : unordered_streams_) {
    DcSctpSocketHandoverState::UnorderedStream state_stream;
    state_stream.id = stream_id.value();
    state.rx.unordered_streams.push_back(std::move(state_stream));
  }
}

}  // namespace dcsctp
