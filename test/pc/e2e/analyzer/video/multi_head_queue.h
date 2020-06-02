/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Stores value in linked list and permits addition to the end and extraction
// from start. Support multiple heads. When data will be extracted from all
// heads it will be removed from the queue.
template <typename T>
class MultiHeadQueue {
 public:
  explicit MultiHeadQueue(int heads_count) : heads_count_(heads_count) {
    RTC_CHECK_LE(heads_count, MultiHeadQueue::kMaxHeadsCount);
    for (int i = 0; i < heads_count_; ++i) {
      heads_.push_back(Head());
    }
  }

  // Add value to the end of the queue. Complexity O(heads_count).
  void PushBack(T value) {
    auto node = std::make_unique<Node>(std::move(value), heads_count_);

    for (size_t i = 0; i < heads_.size(); ++i) {
      if (heads_[i].node == nullptr) {
        heads_[i].node = node.get();
        node->SetHead(static_cast<int>(i));
      }
    }
    if (first_ == nullptr) {
      first_ = std::move(node);
      last_ = first_.get();
    } else {
      last_->next = std::move(node);
      last_ = last_->next.get();
    }
    size_++;
  }

  // Extract element from specified head. Complexity O(1).
  absl::optional<T> PopFront(int index) {
    RTC_CHECK_LT(index, heads_count_);

    if (heads_[index].node == nullptr) {
      return absl::nullopt;
    }

    bool is_first = (heads_[index].node == first_.get());

    T out = heads_[index].node->value;
    heads_[index].node->UnsetHead(index);
    if (heads_[index].node->next != nullptr) {
      heads_[index].node = heads_[index].node->next.get();
      heads_[index].node->SetHead(index);
    } else {
      heads_[index].node = nullptr;
    }

    if (!is_first || first_->HasHeads()) {
      return out;
    }

    // Otherwise we need to remove node from the queue.
    // Make it in 3 steps to keep destruction simpler.
    if (first_->next == nullptr) {
      first_ = nullptr;
      last_ = nullptr;
    } else {
      auto tmp = std::move(first_->next);
      first_ = nullptr;
      first_ = std::move(tmp);
    }
    size_--;

    return out;
  }

  // Returns element at specified head. Complexity O(1).
  absl::optional<T> Front(int index) const {
    RTC_CHECK_LT(index, heads_count_);
    if (heads_[index].node == nullptr) {
      return absl::nullopt;
    }
    return heads_[index].node->value;
  }

  bool IsEmpty() const { return first_ == nullptr; }
  size_t size() const { return size_; }

 private:
  class Node {
   public:
    Node(T value, int heads_count)
        : value(std::move(value)),
          heads_count_(heads_count),
          heads_bitset_(0) {}

    const T value;
    std::unique_ptr<Node> next = nullptr;

    void SetHead(int index) {
      RTC_DCHECK_LE(index, heads_count_);
      uint64_t mask = 1 << index;
      heads_bitset_ |= mask;
    }
    void UnsetHead(int index) {
      RTC_DCHECK_LE(index, heads_count_);
      uint64_t mask = ~(1 << index);
      heads_bitset_ &= mask;
    }
    bool HasHeads() const { return heads_bitset_ > 0; }

   private:
    const int heads_count_;
    uint64_t heads_bitset_;
  };

  struct Head {
    Node* node = nullptr;
  };

  static const int kMaxHeadsCount = 64;

  const int heads_count_;
  std::vector<Head> heads_;

  std::unique_ptr<Node> first_ = nullptr;
  Node* last_ = nullptr;
  size_t size_ = 0;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
