/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/wav_file.h"

#include <errno.h>

#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <utility>

#include "common_audio/include/audio_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace {

static_assert(std::is_trivially_destructible<WavFormat>::value, "");

size_t GetFormatBytesPerSample(WavFormat format) {
  // Only PCM and IEEE Float formats are supported.
  switch (format) {
    case WavFormat::kWavFormatPcm:
      return 2;
    case WavFormat::kWavFormatIeeeFloat:
      return 4;
    case WavFormat::kWavFormatALaw:
    case WavFormat::kWavFormatMuLaw:
    default:
      RTC_CHECK(false) << "Non-implemented wav-format";
      return 2;
  }
}

// Doesn't take ownership of the file handle and won't close it.
class ReadableWavFile : public ReadableWav {
 public:
  explicit ReadableWavFile(FileWrapper* file) : file_(file) {}
  ReadableWavFile(const ReadableWavFile&) = delete;
  ReadableWavFile& operator=(const ReadableWavFile&) = delete;
  size_t Read(void* buf, size_t num_bytes) override {
    size_t count = file_->Read(buf, num_bytes);
    pos_ += count;
    return count;
  }
  bool SeekForward(uint32_t num_bytes) override {
    bool success = file_->SeekRelative(num_bytes);
    if (success) {
      pos_ += num_bytes;
    }
    return success;
  }
  int64_t GetPosition() { return pos_; }

 private:
  FileWrapper* file_;
  int64_t pos_ = 0;
};

}  // namespace

WavReader::WavReader(const std::string& filename)
    : WavReader(FileWrapper::OpenReadOnly(filename)) {}

WavReader::WavReader(FileWrapper file) : file_(std::move(file)) {
  RTC_CHECK(file_.is_open())
      << "Invalid file. Could not create file handle for wav file.";

  ReadableWavFile readable(&file_);
  size_t bytes_per_sample;
  RTC_CHECK(ReadWavHeader(&readable, &num_channels_, &sample_rate_, &format_,
                          &bytes_per_sample, &num_samples_));
  num_samples_remaining_ = num_samples_;
  RTC_CHECK(format_ == WavFormat::kWavFormatPcm ||
            format_ == WavFormat::kWavFormatIeeeFloat)
      << "Non-implemented wav-format";
  RTC_CHECK_EQ(GetFormatBytesPerSample(format_), bytes_per_sample)
      << "Unexpected format mismatch in header";
  data_start_pos_ = readable.GetPosition();
}

void WavReader::Reset() {
  RTC_CHECK(file_.SeekTo(data_start_pos_))
      << "Failed to set position in the file to WAV data start position";
  num_samples_remaining_ = num_samples_;
}

size_t WavReader::ReadSamples(size_t num_samples, int16_t* samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to big-endian when reading from WAV file"
#endif

  constexpr size_t kChunksize = 4096;
  size_t total_num_samples_read = 0;
  size_t num_samples_to_read = 0;
  size_t num_bytes_read = 0;
  size_t num_samples_read = 0;

  while (total_num_samples_read < num_samples &&
         num_samples_to_read == num_samples_read &&
         num_samples_remaining_ > 0) {
    const size_t num_remaining_samples = num_samples - total_num_samples_read;
    num_samples_to_read = std::min(std::min(kChunksize, num_remaining_samples),
                                   num_samples_remaining_);

    if (format_ == WavFormat::kWavFormatIeeeFloat) {
      std::array<float, kChunksize> samples_to_convert;
      num_bytes_read =
          file_.Read(samples_to_convert.data(),
                     num_samples_to_read * sizeof(samples_to_convert[0]));
      num_samples_read = num_bytes_read / sizeof(samples_to_convert[0]);

      for (size_t j = 0; j < num_samples_read; ++j) {
        samples[total_num_samples_read + j] = FloatToS16(samples_to_convert[j]);
      }
    } else {
      RTC_CHECK_EQ(format_, WavFormat::kWavFormatPcm);
      num_bytes_read = file_.Read(&samples[total_num_samples_read],
                                  num_samples_to_read * sizeof(samples[0]));
      num_samples_read = num_bytes_read / sizeof(samples[0]);
    }

    RTC_CHECK(num_samples_read == 0 || (num_bytes_read % num_samples_read) == 0)
        << "Corrupt file: file ended in the middle of a sample.";
    RTC_CHECK(num_samples_read == num_samples_to_read || file_.ReadEof());

    num_samples_remaining_ -= num_samples_read;
    RTC_CHECK_GE(num_samples_remaining_, 0);
    total_num_samples_read += num_samples_read;
  }

  return total_num_samples_read;
}

size_t WavReader::ReadSamples(size_t num_samples, float* samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to big-endian when reading from WAV file"
#endif

  constexpr size_t kChunksize = 4096;
  size_t total_num_samples_read = 0;
  size_t num_samples_to_read = 0;
  size_t num_bytes_read = 0;
  size_t num_samples_read = 0;

  while (total_num_samples_read < num_samples &&
         num_samples_to_read == num_samples_read &&
         num_samples_remaining_ > 0) {
    const size_t num_remaining_samples = num_samples - total_num_samples_read;
    num_samples_to_read = std::min(std::min(kChunksize, num_remaining_samples),
                                   num_samples_remaining_);

    if (format_ == WavFormat::kWavFormatPcm) {
      std::array<int16_t, kChunksize> samples_to_convert;
      num_bytes_read =
          file_.Read(samples_to_convert.data(),
                     num_samples_to_read * sizeof(samples_to_convert[0]));
      num_samples_read = num_bytes_read / sizeof(samples_to_convert[0]);

      for (size_t j = 0; j < num_samples_read; ++j) {
        samples[total_num_samples_read + j] = samples_to_convert[j];
      }
    } else {
      RTC_CHECK_EQ(format_, WavFormat::kWavFormatIeeeFloat);
      num_bytes_read = file_.Read(&samples[total_num_samples_read],
                                  num_samples_to_read * sizeof(samples[0]));
      num_samples_read = num_bytes_read / sizeof(samples[0]);

      for (size_t j = 0; j < num_samples_read; ++j) {
        samples[total_num_samples_read + j] =
            FloatToFloatS16(samples[total_num_samples_read + j]);
      }
    }

    RTC_CHECK(num_samples_read == 0 || (num_bytes_read % num_samples_read) == 0)
        << "Corrupt file: file ended in the middle of a sample.";
    RTC_CHECK(num_samples_read == num_samples_to_read || file_.ReadEof());

    total_num_samples_read += num_samples_read;
    num_samples_remaining_ -= num_samples_read;
    RTC_CHECK_GE(num_samples_remaining_, 0);
  }

  return total_num_samples_read;
}

void WavReader::Close() {
  file_.Close();
}

WavWriter::WavWriter(const std::string& filename,
                     int sample_rate,
                     size_t num_channels,
                     SampleFormats sample_format)
    // Unlike plain fopen, OpenWriteOnly takes care of filename utf8 ->
    // wchar conversion on windows.
    : WavWriter(FileWrapper::OpenWriteOnly(filename),
                sample_rate,
                num_channels,
                sample_format) {}

WavWriter::WavWriter(FileWrapper file,
                     int sample_rate,
                     size_t num_channels,
                     SampleFormats sample_format)
    : sample_rate_(sample_rate),
      num_channels_(num_channels),
      num_samples_(0),
      format_(sample_format == SampleFormats::kInt16
                  ? WavFormat::kWavFormatPcm
                  : WavFormat::kWavFormatIeeeFloat),
      file_(std::move(file)) {
  // Handle errors from the OpenWriteOnly call in above constructor.
  RTC_CHECK(file_.is_open()) << "Invalid file. Could not create wav file.";

  RTC_CHECK(CheckWavParameters(num_channels_, sample_rate_, format_,
                               GetFormatBytesPerSample(format_), num_samples_));

  // Write a blank placeholder header, since we need to know the total number
  // of samples before we can fill in the real data.
  const uint8_t blank_header[MaxWavHeaderSize()] = {0};
  RTC_CHECK(file_.Write(blank_header, WavHeaderSize(format_)));
}

void WavWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to WAV file"
#endif

  constexpr size_t kChunksize = 4096;
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    const size_t num_remaining_samples = num_samples - i;
    const size_t num_samples_to_write =
        std::min(kChunksize, num_remaining_samples);

    if (format_ == WavFormat::kWavFormatPcm) {
      RTC_CHECK(
          file_.Write(&samples[i], sizeof(samples[0]) * num_samples_to_write));
    } else {
      RTC_CHECK_EQ(format_, WavFormat::kWavFormatIeeeFloat);
      float converted_samples[kChunksize];
      for (size_t j = 0; j < num_samples_to_write; ++j) {
        converted_samples[j] = S16ToFloat(samples[i + j]);
      }
      RTC_CHECK(file_.Write(converted_samples, sizeof(converted_samples[0]) *
                                                   num_samples_to_write));
    }

    num_samples_ += num_samples_to_write;
    RTC_CHECK(num_samples_ >= num_samples_to_write);  // detect size_t overflow
  }
}

void WavWriter::WriteSamples(const float* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to WAV file"
#endif
  constexpr size_t kChunksize = 4096;
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    const size_t num_remaining_samples = num_samples - i;
    const size_t num_samples_to_write =
        std::min(kChunksize, num_remaining_samples);

    if (format_ == WavFormat::kWavFormatPcm) {
      int16_t converted_samples[kChunksize];
      for (size_t j = 0; j < num_samples_to_write; ++j) {
        converted_samples[j] = FloatS16ToS16(samples[i + j]);
      }
      RTC_CHECK(file_.Write(converted_samples, sizeof(converted_samples[0]) *
                                                   num_samples_to_write));
    } else {
      RTC_CHECK_EQ(format_, WavFormat::kWavFormatIeeeFloat);
      float converted_samples[kChunksize];
      for (size_t j = 0; j < num_samples_to_write; ++j) {
        converted_samples[j] = FloatS16ToFloat(samples[i + j]);
      }
      RTC_CHECK(file_.Write(converted_samples, sizeof(converted_samples[0]) *
                                                   num_samples_to_write));
    }

    num_samples_ += num_samples_to_write;
    RTC_CHECK(num_samples_ >= num_samples_to_write);  // detect size_t overflow
  }
}

void WavWriter::Close() {
  RTC_CHECK(file_.Rewind());
  uint8_t header[MaxWavHeaderSize()];
  WriteWavHeader(header, num_channels_, sample_rate_, format_,
                 GetFormatBytesPerSample(format_), num_samples_);
  RTC_CHECK(file_.Write(header, WavHeaderSize(format_)));
  RTC_CHECK(file_.Close());
}

}  // namespace webrtc
