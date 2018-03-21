/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <dlfcn.h>

#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>

#include "modules/desktop_capture/mac/screen_capturer_mac_gl.h"

namespace webrtc {

namespace {

const char* kOpenGlLibraryName = "/System/Library/Frameworks/OpenGL.framework/OpenGL";

}  // namespace

// static
bool ScreenCapturerMacGL::IsSupported() {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  return CGDisplayUsesOpenGLAcceleration(mainDevice);
}

ScreenCapturerMacGL::ScreenCapturerMacGL(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : ScreenCapturerMacLegacy(desktop_config_monitor) {}

ScreenCapturerMacGL::~ScreenCapturerMacGL() {
  DestroyContext();
  dlclose(opengl_library_);
}

void ScreenCapturerMacGL::ScreenConfigurationChanged() {
  ScreenCapturerMacLegacy::ScreenConfigurationChanged();

  DestroyContext();

  if (desktop_config_.displays.size() > 1) {
    RTC_LOG(LS_INFO) << "Using CgBlitPreLion (Multi-monitor).";
    return;
  }

  if (!IsSupported()) {
    RTC_LOG(LS_INFO) << "Using CgBlitPreLion (OpenGL unavailable).";
    return;
  }

  opengl_library_ = dlopen(kOpenGlLibraryName, RTLD_LAZY);
  if (!opengl_library_) {
    LOG_F(LS_ERROR) << "Failed to open " << kOpenGlLibraryName;
    abort();
  }

  cgl_set_full_screen_ =
      reinterpret_cast<CGLSetFullScreenFunc>(dlsym(opengl_library_, "CGLSetFullScreen"));
  if (!cgl_set_full_screen_) {
    LOG_F(LS_ERROR);
    abort();
  }

  RTC_LOG(LS_INFO) << "Using GlBlit";

  CGDirectDisplayID mainDevice = CGMainDisplayID();

  CGLPixelFormatAttribute attributes[] = {
// This function does an early return if GetOSVersionName() >= kMacOSLion,
// this code only runs on 10.6 and can be deleted once 10.6 support is
// dropped.  So just keep using kCGLPFAFullScreen even though it was
// deprecated in 10.6 -- it's still functional there, and it's not used on
// newer OS X versions.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      kCGLPFAFullScreen,
#pragma clang diagnostic pop
      kCGLPFADisplayMask,
      (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(mainDevice),
      (CGLPixelFormatAttribute)0};
  CGLPixelFormatObj pixel_format = nullptr;
  GLint matching_pixel_format_count = 0;
  CGLError err = CGLChoosePixelFormat(attributes, &pixel_format, &matching_pixel_format_count);
  assert(err == kCGLNoError);
  err = CGLCreateContext(pixel_format, nullptr, &cgl_context_);
  assert(err == kCGLNoError);
  CGLDestroyPixelFormat(pixel_format);
  (*cgl_set_full_screen_)(cgl_context_);
  CGLSetCurrentContext(cgl_context_);

  size_t buffer_size =
      screen_pixel_bounds_.width() * screen_pixel_bounds_.height() * sizeof(uint32_t);
  pixel_buffer_object_.Init(cgl_context_, buffer_size);
}

bool ScreenCapturerMacGL::Blit(const DesktopFrame& frame,
                               const DesktopRegion& region,
                               bool* needs_flip) {
  if (needs_flip) *needs_flip = false;

  if (cgl_context_) {
    // GL capturers need flipping.
    if (needs_flip) *needs_flip = true;
    if (pixel_buffer_object_.get() != 0) {
      GlBlitFast(frame, region);
    } else {
      // See comment in ScopedPixelBufferObject::Init about why the slow
      // path is always used on 10.5.
      GlBlitSlow(frame);
    }
  } else {
    ScreenCapturerMacLegacy::Blit(frame, region, needs_flip);
  }

  return true;
}

void ScreenCapturerMacGL::GlBlitFast(const DesktopFrame& frame, const DesktopRegion& region) {
  // Clip to the size of our current screen.
  DesktopRect clip_rect = DesktopRect::MakeSize(frame.size());
  if (queue_.previous_frame()) {
    // We are doing double buffer for the capture data so we just need to copy
    // the invalid region from the previous capture in the current buffer.
    // TODO(hclam): We can reduce the amount of copying here by subtracting
    // |capturer_helper_|s region from |last_invalid_region_|.
    // http://crbug.com/92354

    // Since the image obtained from OpenGL is upside-down, need to do some
    // magic here to copy the correct rectangle.
    const int y_offset = (frame.size().height() - 1) * frame.stride();
    for (DesktopRegion::Iterator i(last_invalid_region_); !i.IsAtEnd(); i.Advance()) {
      DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(queue_.previous_frame()->data() + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  last_invalid_region_ = region;

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, frame.size().width(), frame.size().height(), GL_BGRA, GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (!ptr) {
    // If the buffer can't be mapped, assume that it's no longer valid and
    // release it.
    pixel_buffer_object_.Release();
  } else {
    // Copy only from the dirty rects. Since the image obtained from OpenGL is
    // upside-down we need to do some magic here to copy the correct rectangle.
    const int y_offset = (frame.size().height() - 1) * frame.stride();
    for (DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
      DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(ptr + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on
    // GlBlitSlow until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void ScreenCapturerMacGL::GlBlitSlow(const DesktopFrame& frame) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(
      0, 0, frame.size().width(), frame.size().height(), GL_BGRA, GL_UNSIGNED_BYTE, frame.data());
  glPopClientAttrib();
}

void ScreenCapturerMacGL::DestroyContext() {
  if (cgl_context_) {
    pixel_buffer_object_.Release();
    CGLDestroyContext(cgl_context_);
    cgl_context_ = nullptr;
  }
}

}  // namespace webrtc
