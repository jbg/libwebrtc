/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_dictionary.h"

#include <string>

#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace {

void Sync(cricket::StunDictionaryReader& reader,
          cricket::StunDictionaryWriter& writer) {
  int pending = writer.Pending();
  auto delta = writer.CreateDelta();
  if (delta == nullptr) {
    EXPECT_EQ(pending, 0);
  } else {
    EXPECT_NE(pending, 0);
    auto delta_ack = reader.ApplyDelta(*delta);
    if (!delta_ack.ok()) {
      RTC_LOG(LS_ERROR) << "delta_ack.error(): " << delta_ack.error().message();
    }
    EXPECT_TRUE(delta_ack.ok());
    ASSERT_NE(delta_ack.value().first.get(), nullptr);
    writer.ApplyDeltaAck(*delta_ack.value().first);
    EXPECT_FALSE(writer.Pending());
  }
}

void XorToggle(cricket::StunByteStringAttribute& attr, size_t byte) {
  ASSERT_TRUE(attr.length() > byte);
  uint8_t val = attr.GetByte(byte);
  uint8_t new_val = val ^ (128 - (byte & 255));
  attr.SetByte(byte, new_val);
  RTC_LOG(LS_INFO) << "Toggle byte: " << byte
                   << " from: " << std::to_string(val)
                   << " to: " << std::to_string(new_val);
}

std::unique_ptr<cricket::StunByteStringAttribute> Crop(
    const cricket::StunByteStringAttribute& attr,
    int new_length) {
  auto new_attr =
      std::make_unique<cricket::StunByteStringAttribute>(attr.type());
  std::string content = std::string(attr.string_view());
  content.erase(new_length);
  new_attr->CopyBytes(content);
  return new_attr;
}

}  // namespace

namespace cricket {

constexpr int kKey1 = 100;

TEST(StunDictionary, Empty) {
  StunDictionaryReader reader;
  StunDictionaryWriter writer;
  EXPECT_TRUE(reader.empty());
  EXPECT_TRUE(writer.empty());
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_EQ(writer.CreateDelta().get(), nullptr);
}

TEST(StunDictionary, SetAndGet) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(writer.GetUInt64(kKey1), nullptr);
  EXPECT_EQ(writer.GetByteString(kKey1), nullptr);
  EXPECT_EQ(writer.GetAddress(kKey1), nullptr);
  EXPECT_EQ(writer.GetUInt16List(kKey1), nullptr);
}

TEST(StunDictionary, SetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);

  StunDictionaryReader reader;
  EXPECT_TRUE(reader.empty());

  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.bytes_stored(), 12);
}

TEST(StunDictionary, SetSetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt32(kKey1)->SetValue(29);

  StunDictionaryReader reader;
  EXPECT_TRUE(reader.empty());

  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 29u);
  EXPECT_EQ(reader.bytes_stored(), 12);
}

TEST(StunDictionary, SetAndApplyAndSetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);

  StunDictionaryReader reader;
  EXPECT_TRUE(reader.empty());

  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.bytes_stored(), 12);

  writer.SetUInt32(kKey1)->SetValue(29);
  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 29u);
  EXPECT_EQ(reader.bytes_stored(), 12);
}

TEST(StunDictionary, ChangeType) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer.GetUInt32(kKey1)->value(), 27u);

  writer.SetUInt64(kKey1)->SetValue(29);
  EXPECT_EQ(writer.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(writer.GetUInt64(kKey1)->value(), 29ull);
}

TEST(StunDictionary, ChangeTypeApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer.GetUInt32(kKey1)->value(), 27u);

  StunDictionaryReader reader;
  EXPECT_TRUE(reader.empty());
  Sync(reader, writer);
  EXPECT_EQ(writer.GetUInt32(kKey1)->value(), 27u);

  writer.SetUInt64(kKey1)->SetValue(29);
  EXPECT_EQ(writer.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(writer.GetUInt64(kKey1)->value(), 29ull);

  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(reader.GetUInt64(kKey1)->value(), 29ull);
  EXPECT_EQ(reader.bytes_stored(), 16);
}

TEST(StunDictionary, Pending) {
  StunDictionaryWriter writer;
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_FALSE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(29);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1 + 1)->SetValue(31);
  EXPECT_EQ(writer.Pending(), 2);
  EXPECT_TRUE(writer.Pending(kKey1));
  EXPECT_TRUE(writer.Pending(kKey1 + 1));

  StunDictionaryReader reader;

  Sync(reader, writer);
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_FALSE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(32);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));
}

TEST(StunDictionary, Delete) {
  StunDictionaryWriter writer;
  StunDictionaryReader reader;

  writer.SetUInt32(kKey1)->SetValue(27);
  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.bytes_stored(), 12);

  writer.Delete(kKey1);
  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(reader.bytes_stored(), 8);

  writer.Delete(kKey1);
  EXPECT_EQ(writer.Pending(), 0);
}

TEST(StunDictionary, MultiWriter) {
  StunDictionaryWriter writer1;
  StunDictionaryWriter writer2;
  StunDictionaryReader reader;

  writer1.SetUInt32(kKey1)->SetValue(27);
  Sync(reader, writer1);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);

  writer2.SetUInt32(kKey1 + 1)->SetValue(28);
  Sync(reader, writer2);
  EXPECT_EQ(reader.GetUInt32(kKey1 + 1)->value(), 28u);

  writer1.Delete(kKey1);
  Sync(reader, writer1);
  EXPECT_EQ(reader.GetUInt32(kKey1), nullptr);

  writer2.Delete(kKey1 + 1);
  Sync(reader, writer2);
  EXPECT_EQ(reader.GetUInt32(kKey1 + 1), nullptr);
}

TEST(StunDictionary, BytesStored) {
  StunDictionaryWriter writer;
  StunDictionaryReader reader;

  for (int i = 0; i < 10; i++) {
    writer.SetUInt32(kKey1)->SetValue(27);
    writer.SetUInt64(kKey1 + 1)->SetValue(28);
    Sync(reader, writer);
    EXPECT_EQ(reader.bytes_stored(), 28);
    EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
    EXPECT_EQ(reader.GetUInt64(kKey1 + 1)->value(), 28ull);
    writer.Delete(kKey1);
    Sync(reader, writer);
    EXPECT_EQ(reader.bytes_stored(), 24);
    EXPECT_EQ(reader.GetUInt32(kKey1), nullptr);
    EXPECT_EQ(reader.GetUInt64(kKey1 + 1)->value(), 28ull);
    writer.Delete(kKey1 + 1);
    Sync(reader, writer);
    EXPECT_EQ(reader.bytes_stored(), 16);
    EXPECT_EQ(reader.GetUInt32(kKey1), nullptr);
    EXPECT_EQ(reader.GetUInt64(kKey1 + 1), nullptr);
  }
}

TEST(StunDictionary, MaxBytesStored) {
  StunDictionaryWriter writer;
  StunDictionaryReader reader;

  reader.set_max_bytes_stored(30);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  Sync(reader, writer);
  EXPECT_EQ(reader.bytes_stored(), 28);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.GetUInt64(kKey1 + 1)->value(), 28ull);

  writer.SetByteString(kKey1 + 2)->CopyBytes("k");
  {
    auto delta = writer.CreateDelta();
    auto delta_ack = reader.ApplyDelta(*delta);
    EXPECT_FALSE(delta_ack.ok());
    if (!delta_ack.ok()) {
      RTC_LOG(LS_WARNING) << "delta_ack.error(): "
                          << delta_ack.error().message();
    }
  }
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.GetUInt64(kKey1 + 1)->value(), 28ull);
  EXPECT_EQ(reader.GetByteString(kKey1 + 2), nullptr);

  writer.Delete(kKey1 + 1);
  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.GetUInt64(kKey1 + 1), nullptr);
  EXPECT_EQ(reader.GetByteString(kKey1 + 2)->string_view(), "k");
}

TEST(StunDictionary, DataTypes) {
  StunDictionaryWriter writer;
  StunDictionaryReader reader;

  rtc::SocketAddress addr("127.0.0.1", 8080);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  writer.SetAddress(kKey1 + 2)->SetAddress(addr);
  writer.SetByteString(kKey1 + 3)->CopyBytes("keso");
  writer.SetUInt16List(kKey1 + 4)->AddTypeAtIndex(0, 7);

  Sync(reader, writer);
  EXPECT_EQ(reader.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(reader.GetUInt64(kKey1 + 1)->value(), 28ull);
  EXPECT_EQ(reader.GetAddress(kKey1 + 2)->GetAddress(), addr);
  EXPECT_EQ(reader.GetByteString(kKey1 + 3)->string_view(), "keso");
  EXPECT_EQ(reader.GetUInt16List(kKey1 + 4)->GetType(0), 7);
}

TEST(StunDictionary, ParseError) {
  StunDictionaryWriter writer;
  StunDictionaryReader reader;

  rtc::SocketAddress addr("127.0.0.1", 8080);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  writer.SetAddress(kKey1 + 2)->SetAddress(addr);
  writer.SetByteString(kKey1 + 3)->CopyBytes("keso");
  writer.SetUInt16List(kKey1 + 4)->AddTypeAtIndex(0, 7);

  auto delta = writer.CreateDelta();

  // The first 10 bytes are in the header...
  // any modification makes parsing fail.
  for (int i = 0; i < 10; i++) {
    XorToggle(*delta, i);
    EXPECT_FALSE(reader.ApplyDelta(*delta).ok());
    XorToggle(*delta, i);  // toogle back
  }

  // Remove bytes from the delta.
  for (size_t i = 0; i < delta->length(); i++) {
    // The delta does not contain a footer,
    // so it it possible to Crop at special values (attribute boundaries)
    // and apply will still work.
    const std::vector<int> valid_crop_length = {18, 28, 42, 56, 66, 74};
    bool valid = std::find(valid_crop_length.begin(), valid_crop_length.end(),
                           i) != valid_crop_length.end();
    RTC_LOG(LS_INFO) << "Crop delta from: " << delta->length()
                     << " to new length: " << i << " expect ok: " << valid;
    auto cropped_delta = Crop(*delta, i);
    if (valid) {
      EXPECT_TRUE(reader.ApplyDelta(*cropped_delta).ok());
    } else {
      EXPECT_FALSE(reader.ApplyDelta(*cropped_delta).ok());
    }
  }
}

}  // namespace cricket
