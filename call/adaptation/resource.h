/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_H_
#define CALL_ADAPTATION_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // Increasing the load on this resource is desired, if possible.
  kUnderuse,
};

const char* ResourceUsageStateToString(ResourceUsageState usage_state);

class ResourceListener {
 public:
  virtual ~ResourceListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource->usage_state()| is now up-to-date.
  virtual void OnResourceUsageStateMeasured(
      rtc::scoped_refptr<Resource> resource) = 0;
};

class Resource : public rtc::RefCountedObject<rtc::RefCountInterface> {
 public:
  ~Resource() override;

  // All methods on this interface, as well as that of ResourceListener, MUST be
  // invoked on the |resource_adaptation_queue|. The implementation, which may
  // operate on multiple threads/sequences, is responsible for synchronizing
  // states on this task queue; for example, when usage measurements are made.
  virtual void RegisterAdaptationTaskQueue(
      TaskQueueBase* resource_adaptation_queue) = 0;
  // After this call, tasks MUST NOT be posted to |resource_adaptation_queue|
  // and no assumptions must be made whether or not currently pending tasks will
  // get executed.
  virtual void UnregisterAdaptationTaskQueue() = 0;

  // All registered listeners MUST be informed any time UsageState() changes
  // value on the |resource_adaptation_queue|.
  virtual void AddResourceListener(ResourceListener* listener) = 0;
  virtual void RemoveResourceListener(ResourceListener* listener) = 0;

  // Human-readable identifier of this resource.
  virtual std::string Name() const = 0;
  // The latest usage measurement, or null. Within a single task running on the
  // |resource_adaptation_queue|, UsageState() MUST return the same value every
  // time it is called.
  // TODO(https://crbug.com/webrtc/11618): Remove the UsageState() getter in
  // favor of passing the use usage state directly to the ResourceListener. This
  // makes it easier for the implementation not to mess things up (e.g. the
  // requirement that UsageState() returns the same thing if called twice within
  // the same task); implementations will not have to maintain a snapshot
  // internally in case measurements are made off-thread and could change at any
  // time.
  virtual absl::optional<ResourceUsageState> UsageState() const = 0;
  // Clears the UsageState() (making it null) and requires a new measurement be
  // made. This can happen when the load on the system changes and we want to
  // invalidate all measurements.
  virtual void ClearUsageState() = 0;

  // This method allows the Resource to reject a proposed adaptation in the "up"
  // direction if it predicts this would cause overuse of this resource.
  virtual bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) const = 0;

  virtual void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) = 0;

 protected:
  Resource();
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
