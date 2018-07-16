/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_
#define MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_

#include <functional>
#include <memory>
#include <string>

#include "modules/audio_device/win/core_audio_utility_win.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class AudioDeviceBuffer;
class FineAudioBuffer;

namespace webrtc_win {

// Serves as base class for CoreAudioInput and CoreAudioOutput and supports
// device handling and audio streaming where the direction (input or output)
// is set at constructions by the parent.
// The IAudioSessionEvents interface provides notifications of session-related
// events such as changes in the volume level, display name, and session state.
// This class does not use the default ref-counting memory management method
// provided by IUnknown: calling CoreAudioBase::Release() will not delete the
// object. The client will receive notification from the session manager on
// a separate thread owned and controlled by the manager.
class CoreAudioBase : public IAudioSessionEvents {
 public:
  enum class Direction {
    kInput,
    kOutput,
  };

  enum class ErrorType {
    kStreamDisconnected,
  };

  template <typename T>
  auto as_integer(T const value) -> typename std::underlying_type<T>::type {
    return static_cast<typename std::underlying_type<T>::type>(value);
  }

  // Callback definition for notifications of new audio data. For input clients,
  // it means that "new audio data has now been captured", and for output
  // clients, "the output layer now needs new audio data".
  typedef std::function<bool(uint64_t device_frequency)> OnDataCallback;

  // Callback definition for....
  // TODO(henrika): add comments...
  typedef std::function<bool(ErrorType error)> OnErrorCallback;

  void ThreadRun();

  CoreAudioBase(const CoreAudioBase&) = delete;
  CoreAudioBase& operator=(const CoreAudioBase&) = delete;

 protected:
  explicit CoreAudioBase(Direction direction,
                         OnDataCallback data_callback,
                         OnErrorCallback error_callback);
  ~CoreAudioBase();

  std::string GetDeviceID(int index) const;
  int SetDevice(int index);
  int DeviceName(int index, std::string* name, std::string* guid) const;
  bool SwithDeviceIfNeeded();

  // Returns the raw pointer of the ComPtr which holds IAudioClient.
  // Accessing the raw pointer does not increment/decrement the reference count.
  IAudioClient* GetAudioClient() const;

  bool Init();
  bool Start();
  bool Stop();
  bool IsVolumeControlAvailable(bool* available) const;
  bool Restart();

  Direction direction() const { return direction_; }

  // Releases all allocated COM resources.
  void SafeRelease();

  // Returns number of active devices given the specified |direction_|.
  int NumberOfActiveDevices() const;

  // Returns total number of enumerated audio devices which is the sum of all
  // active devices plus two extra (one default and one default
  // communications). The value in |direction_| determines if capture or
  // render devices are counted.
  int NumberOfEnumeratedDevices() const;

  bool IsInput() const;
  bool IsOutput() const;
  bool IsDefaultDevice(int index) const;
  bool IsDefaultCommunicationsDevice(int index) const;
  bool IsDefaultDevice(const std::string& device_id) const;
  bool IsDefaultCommunicationsDevice(const std::string& device_id) const;
  EDataFlow GetDataFlow() const;
  bool IsRestarting() const;
  int64_t TimeSinceStart() const;

  // TODO(henrika): is the existing thread checker in WindowsAudioDeviceModule
  // sufficient?
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker thread_checker_audio_;
  const Direction direction_;
  const OnDataCallback on_data_callback_;
  const OnErrorCallback on_error_callback_;
  AudioDeviceBuffer* audio_device_buffer_ = nullptr;
  bool initialized_ = false;
  std::string device_id_;
  int device_index_;
  WAVEFORMATEXTENSIBLE format_ = {};
  uint32_t endpoint_buffer_size_frames_ = 0;
  // Only one of the IAudioClient pointers will be active, the other two will
  // be nullptr. Version two requires Windows 8 and version 3 needs Windows 10.
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  // IAudioClient2 derives from IAudioClient.
  Microsoft::WRL::ComPtr<IAudioClient2> audio_client2_;
  // IAudioClient3 derives from IAudioClient2.
  Microsoft::WRL::ComPtr<IAudioClient3> audio_client3_;
  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
  ScopedHandle audio_samples_event_;
  ScopedHandle stop_event_;
  ScopedHandle restart_event_;
  std::unique_ptr<rtc::PlatformThread> audio_thread_;
  bool is_active_ = false;
  // Set when restart process starts and cleared when restart stops
  // successfully. Accessed atomically.
  volatile int is_restarting_ = 0;
  int64_t start_time_ = 0;
  int64_t num_data_callbacks_ = 0;
  int latency_ms_ = 0;

 private:
  LONG ref_count_ = 1;

  void StopThread();
  bool HandleRestartEvent();
  AudioSessionState GetAudioSessionState() const;

  // IUnknown (required by IAudioSessionEvents and IMMNotificationClient).
  ULONG __stdcall AddRef() override;
  ULONG __stdcall Release() override;
  HRESULT __stdcall QueryInterface(REFIID iid, void** object) override;

  // IAudioSessionEvents implementation.
  // These methods are called on separate threads owned by the session manager.
  // More than one thread can be involved depending on the type of callback
  // and audio session.
  HRESULT __stdcall OnStateChanged(AudioSessionState new_state) override;
  HRESULT __stdcall OnSessionDisconnected(
      AudioSessionDisconnectReason disconnect_reason) override;
  HRESULT __stdcall OnDisplayNameChanged(LPCWSTR new_display_name,
                                         LPCGUID event_context) override;
  HRESULT __stdcall OnIconPathChanged(LPCWSTR new_icon_path,
                                      LPCGUID event_context) override;
  HRESULT __stdcall OnSimpleVolumeChanged(float new_simple_volume,
                                          BOOL new_mute,
                                          LPCGUID event_context) override;
  HRESULT __stdcall OnChannelVolumeChanged(DWORD channel_count,
                                           float new_channel_volumes[],
                                           DWORD changed_channel,
                                           LPCGUID event_context) override;
  HRESULT __stdcall OnGroupingParamChanged(LPCGUID new_grouping_param,
                                           LPCGUID event_context) override;
};

}  // namespace webrtc_win
}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_WIN_CORE_AUDIO_BASE_WIN_H_
