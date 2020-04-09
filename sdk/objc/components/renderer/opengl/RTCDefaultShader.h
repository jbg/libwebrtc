/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoViewShading.h"

NS_ASSUME_NONNULL_BEGIN

/** Default WebRTCVideoViewShading that will be used in WebRTCNSGLVideoView and
 *  WebRTCEAGLVideoView if no external shader is specified. This shader will render
 *  the video in a rectangle without any color or geometric transformations.
 */
@interface RTCDefaultShader : NSObject<WebRTCVideoViewShading>

@end

NS_ASSUME_NONNULL_END
