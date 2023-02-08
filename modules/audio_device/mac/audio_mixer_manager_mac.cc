/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/mac/audio_mixer_manager_mac.h"

#include <unistd.h>  // getpid()

#include "rtc_base/system/arch.h"

namespace webrtc {

#define WEBRTC_CA_RETURN_ON_ERR(expr)                                \
  do {                                                               \
    err = expr;                                                      \
    if (err != noErr) {                                              \
      logCAMsg(rtc::LS_ERROR, "Error in " #expr, (const char*)&err); \
      return -1;                                                     \
    }                                                                \
  } while (0)

#define WEBRTC_CA_LOG_ERR(expr)                                      \
  do {                                                               \
    err = expr;                                                      \
    if (err != noErr) {                                              \
      logCAMsg(rtc::LS_ERROR, "Error in " #expr, (const char*)&err); \
    }                                                                \
  } while (0)

#define WEBRTC_CA_LOG_WARN(expr)                                       \
  do {                                                                 \
    err = expr;                                                        \
    if (err != noErr) {                                                \
      logCAMsg(rtc::LS_WARNING, "Error in " #expr, (const char*)&err); \
    }                                                                  \
  } while (0)

AudioMixerManagerMac::AudioMixerManagerMac()
    : _inputDeviceID(kAudioObjectUnknown),
      _outputDeviceID(kAudioObjectUnknown),
      _noInputChannels(0),
      _noOutputChannels(0) {
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " created";
}

AudioMixerManagerMac::~AudioMixerManagerMac() {
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " destroyed";
  Close();
}

// ============================================================================
//	                                PUBLIC METHODS
// ============================================================================

int32_t AudioMixerManagerMac::Close() {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  MutexLock lock(&mutex_);

  CloseSpeakerLocked();
  CloseMicrophoneLocked();

  return 0;
}

int32_t AudioMixerManagerMac::CloseSpeaker() {
  MutexLock lock(&mutex_);
  return CloseSpeakerLocked();
}

int32_t AudioMixerManagerMac::CloseSpeakerLocked() {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  _outputDeviceID = kAudioObjectUnknown;
  _noOutputChannels = 0;

  return 0;
}

int32_t AudioMixerManagerMac::CloseMicrophone() {
  MutexLock lock(&mutex_);
  return CloseMicrophoneLocked();
}

int32_t AudioMixerManagerMac::CloseMicrophoneLocked() {
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  _inputDeviceID = kAudioObjectUnknown;
  _noInputChannels = 0;

  return 0;
}

int32_t AudioMixerManagerMac::OpenSpeaker(AudioDeviceID deviceID) {
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerMac::OpenSpeaker(id=" << deviceID
                      << ")";

  MutexLock lock(&mutex_);

  OSStatus err = noErr;
  UInt32 size = 0;
  pid_t hogPid = -1;

  _outputDeviceID = deviceID;

  // Check which process, if any, has hogged the device.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, 0};

  // First, does it have the property? Aggregate devices don't.
  if (AudioObjectHasProperty(_outputDeviceID, &propertyAddress)) {
    size = sizeof(hogPid);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
        _outputDeviceID, &propertyAddress, 0, NULL, &size, &hogPid));

    if (hogPid == -1) {
      RTC_LOG(LS_VERBOSE) << "No process has hogged the output device";
    }
    // getpid() is apparently "always successful"
    else if (hogPid == getpid()) {
      RTC_LOG(LS_VERBOSE) << "Our process has hogged the output device";
    } else {
      RTC_LOG(LS_WARNING) << "Another process (pid = "
                          << static_cast<int>(hogPid)
                          << ") has hogged the output device";

      return -1;
    }
  }

  // get number of channels from stream format
  propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;

  // Get the stream format, to be able to read the number of channels.
  AudioStreamBasicDescription streamFormat;
  size = sizeof(AudioStreamBasicDescription);
  memset(&streamFormat, 0, size);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _outputDeviceID, &propertyAddress, 0, NULL, &size, &streamFormat));

  _noOutputChannels = streamFormat.mChannelsPerFrame;

  return 0;
}

int32_t AudioMixerManagerMac::OpenMicrophone(AudioDeviceID deviceID) {
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerMac::OpenMicrophone(id=" << deviceID
                      << ")";

  MutexLock lock(&mutex_);

  OSStatus err = noErr;
  UInt32 size = 0;
  pid_t hogPid = -1;

  _inputDeviceID = deviceID;

  // Check which process, if any, has hogged the device.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeInput, 0};
  size = sizeof(hogPid);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _inputDeviceID, &propertyAddress, 0, NULL, &size, &hogPid));
  if (hogPid == -1) {
    RTC_LOG(LS_VERBOSE) << "No process has hogged the input device";
  }
  // getpid() is apparently "always successful"
  else if (hogPid == getpid()) {
    RTC_LOG(LS_VERBOSE) << "Our process has hogged the input device";
  } else {
    RTC_LOG(LS_WARNING) << "Another process (pid = " << static_cast<int>(hogPid)
                        << ") has hogged the input device";

    return -1;
  }

  // get number of channels from stream format
  propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;

  // Get the stream format, to be able to read the number of channels.
  AudioStreamBasicDescription streamFormat;
  size = sizeof(AudioStreamBasicDescription);
  memset(&streamFormat, 0, size);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _inputDeviceID, &propertyAddress, 0, NULL, &size, &streamFormat));

  _noInputChannels = streamFormat.mChannelsPerFrame;

  return 0;
}

bool AudioMixerManagerMac::SpeakerIsInitialized() const {
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  return (_outputDeviceID != kAudioObjectUnknown);
}

bool AudioMixerManagerMac::MicrophoneIsInitialized() const {
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  return (_inputDeviceID != kAudioObjectUnknown);
}

int32_t AudioMixerManagerMac::StereoPlayoutIsAvailable(bool& available) {
  if (_outputDeviceID == kAudioObjectUnknown) {
    RTC_LOG(LS_WARNING) << "device ID has not been set";
    return -1;
  }

  available = (_noOutputChannels == 2);
  return 0;
}

int32_t AudioMixerManagerMac::StereoRecordingIsAvailable(bool& available) {
  if (_inputDeviceID == kAudioObjectUnknown) {
    RTC_LOG(LS_WARNING) << "device ID has not been set";
    return -1;
  }

  available = (_noInputChannels == 2);
  return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// CoreAudio errors are best interpreted as four character strings.
void AudioMixerManagerMac::logCAMsg(const rtc::LoggingSeverity sev,
                                    const char* msg,
                                    const char* err) {
  RTC_DCHECK(msg != NULL);
  RTC_DCHECK(err != NULL);
  RTC_DCHECK(sev == rtc::LS_ERROR || sev == rtc::LS_WARNING);

#ifdef WEBRTC_ARCH_BIG_ENDIAN
  switch (sev) {
    case rtc::LS_ERROR:
      RTC_LOG(LS_ERROR) << msg << ": " << err[0] << err[1] << err[2] << err[3];
      break;
    case rtc::LS_WARNING:
      RTC_LOG(LS_WARNING) << msg << ": " << err[0] << err[1] << err[2]
                          << err[3];
      break;
    default:
      break;
  }
#else
  // We need to flip the characters in this case.
  switch (sev) {
    case rtc::LS_ERROR:
      RTC_LOG(LS_ERROR) << msg << ": " << err[3] << err[2] << err[1] << err[0];
      break;
    case rtc::LS_WARNING:
      RTC_LOG(LS_WARNING) << msg << ": " << err[3] << err[2] << err[1]
                          << err[0];
      break;
    default:
      break;
  }
#endif
}

}  // namespace webrtc
// EOF
