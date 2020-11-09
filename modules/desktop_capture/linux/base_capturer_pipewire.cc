/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include <gio/gunixfdlist.h>
#include <glib-object.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/support/type-map.h>

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"

using modules_desktop_capture_linux::InitializeStubs;
using modules_desktop_capture_linux::kModulePipewire;
using modules_desktop_capture_linux::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so";
#endif

// static
void BaseCapturerPipeWire::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_PAUSED:
      pw_stream_set_active(that->pw_stream_, true);
    case PW_STREAM_STATE_STREAMING:
      break;
  }
}

// static
void BaseCapturerPipeWire::OnStreamParamChanged(void* data,
                                                uint32_t id,
                                                const struct spa_pod* param) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

  if (!param || id != SPA_PARAM_Format) {
    pw_stream_update_params(that->pw_stream_, /*params=*/nullptr,
                            /*n_params=*/0);
    return;
  }

  that->spa_video_format_ = new spa_video_info_raw();
  spa_format_video_raw_parse(param, that->spa_video_format_);

  auto width = that->spa_video_format_->size.width;
  auto height = that->spa_video_format_->size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  uint8_t buffer[1024] = {};
  auto builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[2];
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder,
      // id to enumerate buffer requirements
      SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      // Size: specified as integer (i) and set to specified size
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
      // Stride: specified as integer (i) and set to specified stride
      SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
      // Buffers: specifies how many buffers we want to deal with, set as
      // integer (i) where preferred number is 8, then allowed number is defined
      // as range (r) from min and max values and it is undecided (u) to allow
      // negotiation
      SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 2, 64),
      // Align: memory alignment of the buffer, set as integer (i) to specified
      // value
      SPA_PARAM_BUFFERS_align, SPA_POD_Int(16)));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder,
      // id to enumerate supported metadata
      SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
      // Type: specified as id or enum (I)
      SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
      // Size: size of the metadata, specified as integer (i)
      SPA_PARAM_META_size, SPA_POD_Id(sizeof(struct spa_meta_header))));

  pw_stream_update_params(that->pw_stream_, params, /*n_params=*/2);
}

// static
void BaseCapturerPipeWire::OnStreamProcess(void* data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  pw_buffer* buf = nullptr;

  if (!(buf = pw_stream_dequeue_buffer(that->pw_stream_))) {
    return;
  }

  that->HandleBuffer(buf);

  pw_stream_queue_buffer(that->pw_stream_, buf);
}

BaseCapturerPipeWire::BaseCapturerPipeWire(CaptureSourceType source_type)
    : capture_source_type_(source_type) {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  if (spa_video_format_) {
    delete spa_video_format_;
  }

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }

  if (pw_loop_) {
    pw_loop_destroy(pw_loop_);
  }

  if (current_frame_) {
    free(current_frame_);
  }

  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (session_handle_) {
    GDBusMessage* message = g_dbus_message_new_method_call(
        kDesktopBusName, session_handle_, kSessionInterfaceName, "Close");
    if (message) {
      GError* error = nullptr;
      g_dbus_connection_send_message(connection_, message,
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, &error);
      if (error) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
        g_error_free(error);
      }
      g_object_unref(message);
    }
  }

  g_free(start_handle_);
  g_free(sources_handle_);
  g_free(session_handle_);
  g_free(portal_handle_);

  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }

  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
  }
}

void BaseCapturerPipeWire::InitPortal() {
  cancellable_ = g_cancellable_new();
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested), this);
}

void BaseCapturerPipeWire::InitPipeWire() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire library is available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    portal_init_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_loop_ = pw_loop_new(/*properties=*/nullptr);
  pw_main_loop_ =
      pw_thread_loop_new_full(pw_loop_, "pipewire-main-loop", nullptr);

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_context_ = pw_context_new(pw_loop_, NULL, 0);
  pw_core_ = pw_context_connect_fd(pw_context_, pw_fd_, NULL, 0);
  if (pw_core_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Can't connect to pipewire core";
    portal_init_failed_ = true;
    return;
  }

  CreateReceivingStream();

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    portal_init_failed_ = true;
  }

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

void BaseCapturerPipeWire::CreateReceivingStream() {
  spa_rectangle pwMinScreenBounds = spa_rectangle{1, 1};
  spa_rectangle pwScreenBounds =
      spa_rectangle{static_cast<uint32_t>(desktop_size_.width()),
                    static_cast<uint32_t>(desktop_size_.height())};

  spa_fraction pwFrameRateMin = spa_fraction{0, 1};
  spa_fraction pwFrameRateMax = spa_fraction{60, 1};

  pw_properties* prop = pw_properties_new_string(
      "PW_KEY_MEDIA_TYPE=Video PW_KEY_MEDIA_CATEGORY=Capture "
      "PW_KEY_MEDIA_ROLE=Screen");
  pw_stream_ = pw_stream_new(pw_core_, "webrtc-consume-stream", prop);

  uint8_t buffer[1024] = {};
  const spa_pod* params[1];
  struct spa_pod_builder builder;
  spa_pod_builder_init(&builder, buffer, sizeof(buffer));

  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
      SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
      SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
      SPA_FORMAT_VIDEO_format,
      SPA_POD_CHOICE_ENUM_Id(3, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBx,
                             SPA_VIDEO_FORMAT_BGRx),
      SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(&pwScreenBounds, &pwMinScreenBounds,
                                     &pwScreenBounds),
      SPA_FORMAT_VIDEO_framerate,
      SPA_POD_CHOICE_RANGE_Fraction(&pwFrameRateMax, &pwFrameRateMin,
                                    &pwFrameRateMax),
      SPA_FORMAT_VIDEO_maxFramerate,
      SPA_POD_CHOICE_RANGE_Fraction(&pwFrameRateMax, &pwFrameRateMin,
                                    &pwFrameRateMax)));

  pw_stream_add_listener(pw_stream_, &spa_stream_listener_, &pw_stream_events_,
                         this);
  pw_stream_flags flags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE |
      PW_STREAM_FLAG_MAP_BUFFERS);
  if (pw_stream_connect(pw_stream_, PW_DIRECTION_INPUT, /*port_path=*/PW_ID_ANY,
                        flags, params,
                        /*n_params=*/1) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    portal_init_failed_ = true;
    return;
  }
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  spa_buffer* spaBuffer = buffer->buffer;
  void* src = nullptr;

  if (!(src = spaBuffer->datas[0].data)) {
    return;
  }

  uint32_t maxSize = spaBuffer->datas[0].maxsize;
  int32_t srcStride = spaBuffer->datas[0].chunk->stride;
  if (srcStride != (desktop_size_.width() * kBytesPerPixel)) {
    RTC_LOG(LS_ERROR) << "Got buffer with stride different from screen stride: "
                      << srcStride
                      << " != " << (desktop_size_.width() * kBytesPerPixel);
    portal_init_failed_ = true;
    return;
  }

  if (!current_frame_) {
    current_frame_ = static_cast<uint8_t*>(malloc(maxSize));
  }
  RTC_DCHECK(current_frame_ != nullptr);

  // If both sides decided to go with the RGBx format we need to convert it to
  // BGRx to match color format expected by WebRTC.
  if (spa_video_format_->format == SPA_VIDEO_FORMAT_RGBx) {
    uint8_t* tempFrame = static_cast<uint8_t*>(malloc(maxSize));
    std::memcpy(tempFrame, src, maxSize);
    ConvertRGBxToBGRx(tempFrame, maxSize);
    std::memcpy(current_frame_, tempFrame, maxSize);
    free(tempFrame);
  } else {
    std::memcpy(current_frame_, src, maxSize);
  }
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
  // Change color format for KDE KWin which uses RGBx and not BGRx
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

guint BaseCapturerPipeWire::SetupRequestResponseSignal(
    const gchar* object_path,
    GDBusSignalCallback callback) {
  return g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kRequestInterfaceName, "Response",
      object_path, /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      callback, this, /*user_data_free_func=*/nullptr);
}

// static
void BaseCapturerPipeWire::OnProxyRequested(GObject* /*object*/,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GDBusProxy *proxy = g_dbus_proxy_new_finish(result, &error);
  if (!proxy) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }
  that->proxy_ = proxy;
  that->connection_ = g_dbus_proxy_get_connection(that->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";
  that->SessionRequest();
}

// static
gchar* BaseCapturerPipeWire::PrepareSignalHandle(GDBusConnection* connection,
                                                 const gchar* token) {
  gchar* sender = g_strdup(g_dbus_connection_get_unique_name(connection) + 1);
  for (int i = 0; sender[i]; i++) {
    if (sender[i] == '.') {
      sender[i] = '_';
    }
  }

  gchar* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender, "/",
                              token, /*end of varargs*/ nullptr);
  g_free(sender);

  return handle;
}

void BaseCapturerPipeWire::SessionRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string));
  g_free(variant_string);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  portal_handle_ = PrepareSignalHandle(connection_, variant_string);
  session_request_signal_id_ = SetupRequestResponseSignal(
      portal_handle_, OnSessionRequestResponseSignal);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(
      proxy_, "CreateSession", g_variant_new("(a{sv})", &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested), this);
}

// static
void BaseCapturerPipeWire::OnSessionRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->session_request_signal_id_);
      that->session_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void BaseCapturerPipeWire::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  guint32 portal_response;
  GVariant* response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  g_variant_lookup(response_data, "session_handle", "s",
                   &that->session_handle_);
  g_variant_unref(response_data);

  if (!that->session_handle_ || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    that->portal_init_failed_ = true;
    return;
  }

  that->SourcesRequest();
}

void BaseCapturerPipeWire::SourcesRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(&builder, "{sv}", "types",
                        g_variant_new_uint32(capture_source_type_));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  sources_handle_ = PrepareSignalHandle(connection_, variant_string);
  sources_request_signal_id_ = SetupRequestResponseSignal(
      sources_handle_, OnSourcesRequestResponseSignal);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "SelectSources",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), this);
}

// static
void BaseCapturerPipeWire::OnSourcesRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->sources_request_signal_id_);
      that->sources_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void BaseCapturerPipeWire::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  that->StartRequest();
}

void BaseCapturerPipeWire::StartRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  start_handle_ = PrepareSignalHandle(connection_, variant_string);
  start_request_signal_id_ =
      SetupRequestResponseSignal(start_handle_, OnStartRequestResponseSignal);
  g_free(variant_string);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const gchar parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "Start",
      g_variant_new("(osa{sv})", session_handle_, parent_window, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnStartRequested), this);
}

// static
void BaseCapturerPipeWire::OnStartRequested(GDBusProxy *proxy,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (that->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->start_request_signal_id_);
      that->start_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void BaseCapturerPipeWire::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Start signal received.";
  guint32 portal_response;
  GVariant* response_data;
  GVariantIter* iter = nullptr;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data, "streams", "a(ua{sv})", &iter)) {
    GVariant* variant;

    while (g_variant_iter_next(iter, "@(ua{sv})", &variant)) {
      guint32 stream_id;
      gint32 width;
      gint32 height;
      GVariant* options;

      g_variant_get(variant, "(u@a{sv})", &stream_id, &options);
      RTC_DCHECK(options != nullptr);

      g_variant_lookup(options, "size", "(ii)", &width, &height);

      that->desktop_size_.set(width, height);

      g_variant_unref(options);
      g_variant_unref(variant);
    }
  }
  g_variant_iter_free(iter);
  g_variant_unref(response_data);

  that->OpenPipeWireRemote();
}

void BaseCapturerPipeWire::OpenPipeWireRemote() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      this);
}

// static
void BaseCapturerPipeWire::OnOpenPipeWireRemoteRequested(
    GDBusProxy *proxy,
    GAsyncResult* result,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GUnixFDList* outlist = nullptr;
  GVariant* variant = g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, &outlist, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  gint32 index;
  g_variant_get(variant, "(h)", &index);

  if ((that->pw_fd_ = g_unix_fd_list_get(outlist, index, &error)) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    g_error_free(error);
    g_variant_unref(variant);
    that->portal_init_failed_ = true;
    return;
  }

  g_variant_unref(variant);
  g_object_unref(outlist);

  that->InitPipeWire();
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  InitPortal();

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (portal_init_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!current_frame_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(desktop_size_));
  result->CopyPixelsFrom(
      current_frame_, (desktop_size_.width() * kBytesPerPixel),
      DesktopRect::MakeWH(desktop_size_.width(), desktop_size_.height()));
  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal.
  // But we have to add an empty source as the code expects it.
  sources->push_back({0});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return true;
}

}  // namespace webrtc
