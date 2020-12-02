/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_H_

#include <AudioUnit/AudioUnit.h>

namespace webrtc {
namespace ios_adm {

class VoiceProcessingAudioUnitObserver {
 public:
  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to signal that recorded audio is available.
  virtual OSStatus OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                         const AudioTimeStamp* time_stamp,
                                         UInt32 bus_number,
                                         UInt32 num_frames,
                                         AudioBufferList* io_data) = 0;

  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to provide audio samples to the audio unit.
  virtual OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* io_action_flags,
                                    const AudioTimeStamp* time_stamp,
                                    UInt32 bus_number,
                                    UInt32 num_frames,
                                    AudioBufferList* io_data) = 0;

 protected:
  ~VoiceProcessingAudioUnitObserver() {}
};

// Convenience class to abstract away the management of a Voice Processing
// I/O Audio Unit. The Voice Processing I/O unit has the same characteristics
// as the Remote I/O unit (supports full duplex low-latency audio input and
// output) and adds AEC for for two-way duplex communication. It also adds AGC,
// adjustment of voice-processing quality, and muting. Hence, ideal for
// VoIP applications.
class VoiceProcessingAudioUnit {
 public:
  virtual ~VoiceProcessingAudioUnit() {}

  // TODO(tkchin): enum for state and state checking.
  enum State : int32_t {
    // Init() should be called.
    kInitRequired,
    // Audio unit created but not initialized.
    kUninitialized,
    // Initialized but not started. Equivalent to stopped.
    kInitialized,
    // Initialized and started.
    kStarted,
  };

  // Number of bytes per audio sample for 16-bit signed integer representation.
  static const UInt32 kBytesPerSample = 2;

  // Initializes this class by creating the underlying audio unit instance.
  // Creates a Voice-Processing I/O unit and configures it for full-duplex
  // audio. The selected stream format is selected to avoid internal resampling
  // and to match the 10ms callback rate for WebRTC as well as possible.
  // Does not intialize the audio unit.
  virtual bool Init() = 0;

  virtual VoiceProcessingAudioUnit::State GetState() const = 0;

  // Initializes the underlying audio unit with the given sample rate.
  virtual bool Initialize(Float64 sample_rate) = 0;

  // Starts the underlying audio unit.
  virtual bool Start() = 0;

  // Stops the underlying audio unit.
  virtual bool Stop() = 0;

  // Uninitializes the underlying audio unit.
  virtual bool Uninitialize() = 0;

  // Calls render on the underlying audio unit.
  virtual OSStatus Render(AudioUnitRenderActionFlags* flags,
                          const AudioTimeStamp* time_stamp,
                          UInt32 output_bus_number, UInt32 num_frames,
                          AudioBufferList* io_data) = 0;

  // Set to 1 to mute microphone through the CoreAudio AudioUnit, 0 otherwise.
  virtual int32_t SetMicrophoneMute(bool enable) = 0;
  virtual int32_t MicrophoneMute(bool& enabled) const = 0;
};

}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_H_
