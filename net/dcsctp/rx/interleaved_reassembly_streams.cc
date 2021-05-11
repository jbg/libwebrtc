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
  const auto start_iter = chunk_map.begin();
  const auto end_iter = chunk_map.upper_bound(mid);

  size_t removed_bytes =
      std::accumulate(start_iter, end_iter, 0, [](size_t r1, const auto& p) {
        return r1 +
               absl::c_accumulate(p.second, 0, [](size_t r2, const auto& q) {
                 return r2 + q.second.second.size();
               });
      });

  chunk_map.erase(start_iter, end_iter);
  return removed_bytes;
}
}  // namespace

size_t InterleavedReassemblyStreams::StreamBase::TryToAssembleMessage(
    UnwrappedMID mid) {
  const ChunkMap& tsn_chunks = chunks_by_mid_[mid];
  if (tsn_chunks.empty()) {
    return 0;
  }
  if (!tsn_chunks.begin()->second.second.is_beginning ||
      !tsn_chunks.rbegin()->second.second.is_end) {
    RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "Missing beginning or end";
    return 0;
  }
  int64_t fsn_diff = *tsn_chunks.rbegin()->first - *tsn_chunks.begin()->first;
  if (fsn_diff != (static_cast<ssize_t>(tsn_chunks.size()) - 1)) {
    RTC_DLOG(LS_VERBOSE) << parent_.log_prefix_ << "Not all chunks exist (have "
                         << tsn_chunks.size() << ", expect " << (fsn_diff + 1)
                         << ")";
    return 0;
  }

  size_t removed_bytes = AssembleMessage(tsn_chunks);

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
  auto p =
      chunks_by_mid_[mid].emplace(fsn, std::make_pair(tsn, std::move(data)));
  if (!p.second /* inserted */) {
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
  auto p =
      chunks_by_mid_[mid].emplace(fsn, std::make_pair(tsn, std::move(data)));
  if (!p.second /* inserted */) {
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
    auto it = unordered_chunks_.emplace(data.stream_id, this).first;
    return it->second.Add(tsn, std::move(data));
  }

  auto it = ordered_chunks_.emplace(data.stream_id, this).first;
  return it->second.Add(tsn, std::move(data));
}

size_t InterleavedReassemblyStreams::HandleForwardTsn(
    UnwrappedTSN new_cumulative_ack_tsn,
    rtc::ArrayView<const AnyForwardTsnChunk::SkippedStream> skipped_streams) {
  size_t removed_bytes = 0;
  for (const auto& skipped_stream : skipped_streams) {
    if (skipped_stream.unordered) {
      auto it = unordered_chunks_.find(skipped_stream.stream_id);
      if (it != unordered_chunks_.end()) {
        removed_bytes += it->second.EraseTo(skipped_stream.message_id);
      } else {
        RTC_DLOG(LS_VERBOSE)
            << log_prefix_ << "I-FORWARD-TSN reference unordered stream "
            << *skipped_stream.stream_id << " that does not exist";
      }
    } else {
      auto it = ordered_chunks_.find(skipped_stream.stream_id);
      if (it != ordered_chunks_.end()) {
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
    for (auto& entry : ordered_chunks_) {
      entry.second.Reset();
    }
    for (auto& entry : unordered_chunks_) {
      entry.second.Reset();
    }
  } else {
    for (StreamID stream_id : stream_ids) {
      auto ordered_it = ordered_chunks_.find(stream_id);
      if (ordered_it != ordered_chunks_.end()) {
        ordered_it->second.Reset();
      }
      auto unordered_it = unordered_chunks_.find(stream_id);
      if (unordered_it != unordered_chunks_.end()) {
        unordered_it->second.Reset();
      }
    }
  }
}
}  // namespace dcsctp
