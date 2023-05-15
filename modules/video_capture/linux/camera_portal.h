/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_LINUX_CAMERA_PORTAL_H_
#define MODULES_VIDEO_CAPTURE_LINUX_CAMERA_PORTAL_H_

#include <gio/gio.h>

#include <string>

#include "modules/portal/portal_request_response.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT CameraPortal {
 public:
  class PortalNotifier {
   public:
    virtual void OnCameraRequestResult(xdg_portal::RequestResponse result,
                                       int fd) = 0;

   protected:
    PortalNotifier() = default;
    virtual ~PortalNotifier() = default;
  };

  explicit CameraPortal(PortalNotifier* notifier);
  ~CameraPortal();

  void Start();

 private:
  void OnPortalDone(xdg_portal::RequestResponse result);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);
  void ProxyRequested(GDBusProxy* proxy);

  static void OnAccessResponse(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnResponseSignalEmitted(GDBusConnection* connection,
                                      const char* sender_name,
                                      const char* object_path,
                                      const char* interface_name,
                                      const char* signal_name,
                                      GVariant* parameters,
                                      gpointer user_data);
  static void OnOpenResponse(GDBusProxy* proxy,
                             GAsyncResult* result,
                             gpointer user_data);

  CameraPortal::PortalNotifier* notifier_ = nullptr;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  guint access_request_signal_id_ = 0;
  int pw_fd_ = -1;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_LINUX_CAMERA_PORTAL_H_
