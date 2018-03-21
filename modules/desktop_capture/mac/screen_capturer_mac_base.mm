/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/screen_capturer_mac_base.h"

// Once Chrome no longer supports OSX 10.8, everything within this
// preprocessor block can be removed. https://crbug.com/579255
#if !defined(MAC_OS_X_VERSION_10_9) || MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
CG_EXTERN const CGRect* CGDisplayStreamUpdateGetRects(CGDisplayStreamUpdateRef updateRef,
                                                      CGDisplayStreamUpdateRectType rectType,
                                                      size_t* rectCount);
CG_EXTERN CFRunLoopSourceRef CGDisplayStreamGetRunLoopSource(CGDisplayStreamRef displayStream);
CG_EXTERN CGError CGDisplayStreamStop(CGDisplayStreamRef displayStream);
CG_EXTERN CGError CGDisplayStreamStart(CGDisplayStreamRef displayStream);
CG_EXTERN CGDisplayStreamRef CGDisplayStreamCreate(CGDirectDisplayID display,
                                                   size_t outputWidth,
                                                   size_t outputHeight,
                                                   int32_t pixelFormat,
                                                   CFDictionaryRef properties,
                                                   CGDisplayStreamFrameAvailableHandler handler);
#endif

namespace webrtc {

namespace {

// Standard Mac displays have 72dpi, but we report 96dpi for
// consistency with Windows and Linux.
const int kStandardDPI = 96;

// DesktopFrame wrapper that flips wrapped frame upside down by inverting
// stride.
class InvertedDesktopFrame : public DesktopFrame {
 public:
  InvertedDesktopFrame(std::unique_ptr<DesktopFrame> frame)
      : DesktopFrame(frame->size(),
                     -frame->stride(),
                     frame->data() + (frame->size().height() - 1) * frame->stride(),
                     frame->shared_memory()) {
    original_frame_ = std::move(frame);
    set_dpi(original_frame_->dpi());
    set_capture_time_ms(original_frame_->capture_time_ms());
    mutable_updated_region()->Swap(original_frame_->mutable_updated_region());
  }
  ~InvertedDesktopFrame() override {}

 private:
  std::unique_ptr<DesktopFrame> original_frame_;

  RTC_DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

}  // namespace

// CGDisplayStreamRefs need to be destroyed asynchronously after receiving a
// kCGDisplayStreamFrameStatusStopped callback from CoreGraphics. This may
// happen after the ScreenCapturerMac has been destroyed. DisplayStreamManager
// is responsible for destroying all extant CGDisplayStreamRefs, and will
// destroy itself once it's done.
class DisplayStreamManager {
 public:
  int GetUniqueId() { return ++unique_id_generator_; }
  void DestroyStream(int unique_id) {
    auto it = display_stream_wrappers_.find(unique_id);
    RTC_CHECK(it != display_stream_wrappers_.end());
    RTC_CHECK(!it->second.active);
    CFRelease(it->second.stream);
    display_stream_wrappers_.erase(it);

    if (ready_for_self_destruction_ && display_stream_wrappers_.empty()) delete this;
  }

  void SaveStream(int unique_id, CGDisplayStreamRef stream) {
    RTC_CHECK(unique_id <= unique_id_generator_);
    DisplayStreamWrapper wrapper;
    wrapper.stream = stream;
    display_stream_wrappers_[unique_id] = wrapper;
  }

  void UnregisterActiveStreams() {
    for (auto& pair : display_stream_wrappers_) {
      DisplayStreamWrapper& wrapper = pair.second;
      if (wrapper.active) {
        wrapper.active = false;
        CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(wrapper.stream);
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
        CGDisplayStreamStop(wrapper.stream);
      }
    }
  }

  void PrepareForSelfDestruction() {
    ready_for_self_destruction_ = true;

    if (display_stream_wrappers_.empty()) delete this;
  }

  // Once the DisplayStreamManager is ready for destruction, the
  // ScreenCapturerMac is no longer present. Any updates should be ignored.
  bool ShouldIgnoreUpdates() { return ready_for_self_destruction_; }

 private:
  struct DisplayStreamWrapper {
    // The registered CGDisplayStreamRef.
    CGDisplayStreamRef stream = nullptr;

    // Set to false when the stream has been stopped. An asynchronous callback
    // from CoreGraphics will let us destroy the CGDisplayStreamRef.
    bool active = true;
  };

  std::map<int, DisplayStreamWrapper> display_stream_wrappers_;
  int unique_id_generator_ = 0;
  bool ready_for_self_destruction_ = false;
};

ScreenCapturerMacBase::ScreenCapturerMacBase(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : desktop_config_monitor_(desktop_config_monitor) {
  display_stream_manager_ = new DisplayStreamManager;
}

ScreenCapturerMacBase::~ScreenCapturerMacBase() {
  ReleaseBuffers();
  UnregisterRefreshAndMoveHandlers();
  display_stream_manager_->PrepareForSelfDestruction();
}

bool ScreenCapturerMacBase::Init() {
  desktop_config_monitor_->Lock();
  desktop_config_ = desktop_config_monitor_->desktop_configuration();
  desktop_config_monitor_->Unlock();
  if (!RegisterRefreshAndMoveHandlers()) {
    return false;
  }
  ScreenConfigurationChanged();
  return true;
}

void ScreenCapturerMacBase::ReleaseBuffers() {
  // The buffers might be in use by the encoder, so don't delete them here.
  // Instead, mark them as "needs update"; next time the buffers are used by
  // the capturer, they will be recreated if necessary.
  queue_.Reset();
}

void ScreenCapturerMacBase::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void ScreenCapturerMacBase::CaptureFrame() {
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  queue_.MoveToNextFrame();
  RTC_DCHECK(!queue_.current_frame() || !queue_.current_frame()->IsShared());

  desktop_config_monitor_->Lock();
  MacDesktopConfiguration new_config = desktop_config_monitor_->desktop_configuration();
  if (!desktop_config_.Equals(new_config)) {
    desktop_config_ = new_config;
    // If the display configuraiton has changed then refresh capturer data
    // structures. Occasionally, the refresh and move handlers are lost when
    // the screen mode changes, so re-register them here.
    UnregisterRefreshAndMoveHandlers();
    RegisterRefreshAndMoveHandlers();
    ScreenConfigurationChanged();
  }

  DesktopRegion region;
  helper_.TakeInvalidRegion(&region);

  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame()) queue_.ReplaceCurrentFrame(SharedDesktopFrame::Wrap(CreateFrame()));

  DesktopFrame* current_frame = queue_.current_frame();

  bool needs_flip = false;  // GL capturers need flipping.
  bool result = Blit(*current_frame, region, &needs_flip);
  if (!result) {
    desktop_config_monitor_->Unlock();
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  std::unique_ptr<DesktopFrame> new_frame = queue_.current_frame()->Share();
  *new_frame->mutable_updated_region() = region;

  if (needs_flip) new_frame.reset(new InvertedDesktopFrame(std::move(new_frame)));

  helper_.set_size_most_recent(new_frame->size());

  // Signal that we are done capturing data from the display framebuffer,
  // and accessing display structures.
  desktop_config_monitor_->Unlock();

  new_frame->set_capture_time_ms((rtc::TimeNanos() - capture_start_time_nanos) /
                                 rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(new_frame));
}

bool ScreenCapturerMacBase::GetSourceList(SourceList* screens) {
  assert(screens->size() == 0);
  if (rtc::GetOSVersionName() < rtc::kMacOSLion) {
    // Single monitor cast is not supported on pre OS X 10.7.
    screens->push_back({kFullDesktopScreenId});
    return true;
  }

  for (MacDisplayConfigurations::iterator it = desktop_config_.displays.begin();
       it != desktop_config_.displays.end();
       ++it) {
    screens->push_back({it->id});
  }
  return true;
}

bool ScreenCapturerMacBase::SelectSource(SourceId id) {
  if (rtc::GetOSVersionName() < rtc::kMacOSLion) {
    // Ignore the screen selection on unsupported OS.
    assert(!current_display_);
    return id == kFullDesktopScreenId;
  }

  if (id == kFullDesktopScreenId) {
    current_display_ = 0;
  } else {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(static_cast<CGDirectDisplayID>(id));
    if (!config) return false;
    current_display_ = config->id;
  }

  ScreenConfigurationChanged();
  return true;
}

void ScreenCapturerMacBase::ScreenConfigurationChanged() {
  if (current_display_) {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    screen_pixel_bounds_ = config ? config->pixel_bounds : DesktopRect();
    dip_to_pixel_scale_ = config ? config->dip_to_pixel_scale : 1.0f;
  } else {
    screen_pixel_bounds_ = desktop_config_.pixel_bounds;
    dip_to_pixel_scale_ = desktop_config_.dip_to_pixel_scale;
  }

  // Release existing buffers, which will be of the wrong size.
  ReleaseBuffers();

  // Clear the dirty region, in case the display is down-sizing.
  helper_.ClearInvalidRegion();

  // Re-mark the entire desktop as dirty.
  helper_.InvalidateScreen(screen_pixel_bounds_.size());
}

bool ScreenCapturerMacBase::RegisterRefreshAndMoveHandlers() {
  desktop_config_ = desktop_config_monitor_->desktop_configuration();
  for (const auto& config : desktop_config_.displays) {
    size_t pixel_width = config.pixel_bounds.width();
    size_t pixel_height = config.pixel_bounds.height();
    if (pixel_width == 0 || pixel_height == 0) continue;
    // Using a local variable forces the block to capture the raw pointer.
    DisplayStreamManager* manager = display_stream_manager_;
    int unique_id = manager->GetUniqueId();
    CGDirectDisplayID display_id = config.id;
    DesktopVector display_origin = config.pixel_bounds.top_left();

    CGDisplayStreamFrameAvailableHandler handler = ^(CGDisplayStreamFrameStatus status,
                                                     uint64_t display_time,
                                                     IOSurfaceRef frame_surface,
                                                     CGDisplayStreamUpdateRef updateRef) {
      if (status == kCGDisplayStreamFrameStatusStopped) {
        manager->DestroyStream(unique_id);
        return;
      }

      if (manager->ShouldIgnoreUpdates()) return;

      // Only pay attention to frame updates.
      if (status != kCGDisplayStreamFrameStatusFrameComplete) return;

      size_t count = 0;
      const CGRect* rects =
          CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateDirtyRects, &count);
      if (count != 0) {
        // According to CGDisplayStream.h, it's safe to call
        // CGDisplayStreamStop() from within the callback.
        ScreenRefresh(count, rects, display_origin, display_id, frame_surface);
      }
    };

    rtc::ScopedCFTypeRef<CFDictionaryRef> properties_dict(
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void* []){kCGDisplayStreamShowCursor},
                           (const void* []){kCFBooleanFalse},
                           1,
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks));

    CGDisplayStreamRef display_stream = CGDisplayStreamCreate(
        display_id, pixel_width, pixel_height, 'BGRA', properties_dict.get(), handler);

    if (display_stream) {
      CGError error = CGDisplayStreamStart(display_stream);
      if (error != kCGErrorSuccess) return false;

      CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(display_stream);
      CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      display_stream_manager_->SaveStream(unique_id, display_stream);
    }
  }

  return true;
}

void ScreenCapturerMacBase::UnregisterRefreshAndMoveHandlers() {
  display_stream_manager_->UnregisterActiveStreams();
}

void ScreenCapturerMacBase::ScreenRefresh(CGRectCount count,
                                          const CGRect* rect_array,
                                          DesktopVector display_origin,
                                          CGDirectDisplayID display_id,
                                          IOSurfaceRef io_surface) {
  if (screen_pixel_bounds_.is_empty()) ScreenConfigurationChanged();

  // The refresh rects are in display coordinates. We want to translate to
  // framebuffer coordinates. If a specific display is being captured, then no
  // change is necessary. If all displays are being captured, then we want to
  // translate by the origin of the display.
  DesktopVector translate_vector;
  if (!current_display_) translate_vector = display_origin;

  DesktopRegion region;
  for (CGRectCount i = 0; i < count; ++i) {
    // All rects are already in physical pixel coordinates.
    DesktopRect rect = DesktopRect::MakeXYWH(rect_array[i].origin.x,
                                             rect_array[i].origin.y,
                                             rect_array[i].size.width,
                                             rect_array[i].size.height);

    rect.Translate(translate_vector);

    region.AddRect(rect);
  }

  helper_.InvalidateRegion(region);
}

std::unique_ptr<DesktopFrame> ScreenCapturerMacBase::CreateFrame() {
  std::unique_ptr<DesktopFrame> frame(new BasicDesktopFrame(screen_pixel_bounds_.size()));
  frame->set_dpi(
      DesktopVector(kStandardDPI * dip_to_pixel_scale_, kStandardDPI * dip_to_pixel_scale_));
  return frame;
}

// Copy pixels in the |rect| from |src_place| to |dest_plane|. |rect| should be
// relative to the origin of |src_plane| and |dest_plane|.
void ScreenCapturerMacBase::CopyRect(const uint8_t* src_plane,
                                     int src_plane_stride,
                                     uint8_t* dest_plane,
                                     int dest_plane_stride,
                                     int bytes_per_pixel,
                                     const DesktopRect& rect) const {
  // Get the address of the starting point.
  const int src_y_offset = src_plane_stride * rect.top();
  const int dest_y_offset = dest_plane_stride * rect.top();
  const int x_offset = bytes_per_pixel * rect.left();
  src_plane += src_y_offset + x_offset;
  dest_plane += dest_y_offset + x_offset;

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = bytes_per_pixel * rect.width();
  const int height = rect.height();
  for (int i = 0; i < height; ++i) {
    memcpy(dest_plane, src_plane, bytes_per_line);
    src_plane += src_plane_stride;
    dest_plane += dest_plane_stride;
  }
}

}  // namespace webrtc
