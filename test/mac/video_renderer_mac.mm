/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/mac/video_renderer_mac.h"
#include "api/video/video_frame.h"

#import "sdk/objc/api/video_frame_buffer/RTCNativeI420Buffer+Private.h"
#import "sdk/objc/base/RTCVideoFrame.h"
#import "sdk/objc/components/renderer/metal/RTCMTLNSVideoView.h"

#import <Cocoa/Cocoa.h>

// Creates a Cocoa Window with a RTCMTLNSVideoView view.
@interface CocoaWindow : NSObject {
 @private
  NSWindow *window_;
  RTC_OBJC_TYPE(RTCMTLNSVideoView) * view_;
  NSString *title_;
  int width_;
  int height_;
}

- (id)initWithTitle:(NSString *)title width:(int)width height:(int)height;
// 'createWindow' must be called on the main thread.
- (void)createWindow:(NSObject *)ignored;

- (void)onFrame:(const webrtc ::VideoFrame&)frame;

@end

@implementation CocoaWindow
  static NSInteger nextXOrigin_;
  static NSInteger nextYOrigin_;

- (id)initWithTitle:(NSString *)title width:(int)width height:(int)height {
  if (self = [super init]) {
    title_ = title;
    width_ = width;
    height_ = height;
  }
  return self;
}

- (void)createWindow:(NSObject *)ignored {
  NSInteger xOrigin = nextXOrigin_;
  NSRect screenFrame = [[NSScreen mainScreen] frame];
  if (nextXOrigin_ + width_ < screenFrame.size.width) {
    nextXOrigin_ += width_;
  } else {
    xOrigin = 0;
    nextXOrigin_ = 0;
    nextYOrigin_ += height_;
  }
  if (nextYOrigin_ + height_ > screenFrame.size.height) {
    xOrigin = 0;
    nextXOrigin_ = 0;
    nextYOrigin_ = 0;
  }
  NSInteger yOrigin = nextYOrigin_;
  NSRect windowFrame = NSMakeRect(xOrigin, yOrigin, width_, height_);
  window_ = [[NSWindow alloc] initWithContentRect:windowFrame
                                        styleMask:NSWindowStyleMaskTitled
                                          backing:NSBackingStoreBuffered
                                            defer:NO];

  NSRect viewFrame = NSMakeRect(0, 0, width_, height_);
  view_ = [[RTC_OBJC_TYPE(RTCMTLNSVideoView) alloc] initWithFrame:viewFrame];

  [[window_ contentView] addSubview:view_];
  [window_ setTitle:title_];
  [window_ makeKeyAndOrderFront:NSApp];
}

- (void)onFrame:(const webrtc::VideoFrame&)frame {
  RTC_OBJC_TYPE(RTCI420Buffer)* buffer = [[RTC_OBJC_TYPE(RTCI420Buffer) alloc]
      initWithFrameBuffer:frame.video_frame_buffer()->ToI420()];
  RTC_OBJC_TYPE(RTCVideoFrame)* videoFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:buffer
            rotation:static_cast<RTCVideoRotation>(frame.rotation())
         timeStampNs:frame.timestamp_us()];
  [view_ renderFrame:videoFrame];
}

@end

namespace webrtc {
namespace test {

VideoRenderer* VideoRenderer::CreatePlatformRenderer(const char* window_title,
                                                     size_t width,
                                                     size_t height) {
  MacRenderer* renderer = new MacRenderer();
  if (!renderer->Init(window_title, width, height)) {
    delete renderer;
    return NULL;
  }
  return renderer;
}

MacRenderer::MacRenderer()
    : window_(NULL) {}

MacRenderer::~MacRenderer() {}

bool MacRenderer::Init(const char* window_title, int width, int height) {
  window_ = [[CocoaWindow alloc]
      initWithTitle:[NSString stringWithUTF8String:window_title]
                                             width:width
                                            height:height];
  if (!window_)
    return false;
  [window_ performSelectorOnMainThread:@selector(createWindow:) withObject:nil waitUntilDone:YES];
  return true;
}

void MacRenderer::OnFrame(const VideoFrame& frame) {
  [window_ onFrame:frame];
}

}  // test
}  // webrtc
