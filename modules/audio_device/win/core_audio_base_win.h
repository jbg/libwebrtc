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
// object.
class CoreAudioBase : public IAudioSessionEvents {
 public:
  enum class Direction {
    kInput,
    kOutput,
  };

  // Callback definition for notifications of new audio data. For input clients,
  // it means that "new audio data has now been captured", and for output
  // clients, "the output layer now needs new audio data".
  typedef std::function<bool(uint64_t device_frequency)> OnDataCallback;

  explicit CoreAudioBase(Direction direction, OnDataCallback callback);
  ~CoreAudioBase();

  std::string GetDeviceID(int index) const;
  int DeviceName(int index, std::string* name, std::string* guid) const;

  bool Init();
  bool Start();
  bool Stop();
  bool IsVolumeControlAvailable(bool* available) const;

  Direction direction() const { return direction_; }

  void ThreadRun();

  CoreAudioBase(const CoreAudioBase&) = delete;
  CoreAudioBase& operator=(const CoreAudioBase&) = delete;

 protected:
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

  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker thread_checker_audio_;
  const Direction direction_;
  const OnDataCallback on_data_callback_;
  AudioDeviceBuffer* audio_device_buffer_ = nullptr;
  bool initialized_ = false;
  std::string device_id_;
  WAVEFORMATEXTENSIBLE format_ = {};
  uint32_t endpoint_buffer_size_frames_ = 0;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
  ScopedHandle audio_samples_event_;
  ScopedHandle stop_event_;
  std::unique_ptr<rtc::PlatformThread> audio_thread_;

 private:
  LONG ref_count_ = 1;

  void StopThread();
  AudioSessionState GetAudioSessionState() const;

  // IUnknown (required by IAudioSessionEvents and IMMNotificationClient).
  ULONG __stdcall AddRef() override;
  ULONG __stdcall Release() override;
  HRESULT __stdcall QueryInterface(REFIID iid, void** object) override;

  // IAudioSessionEvents implementation
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
