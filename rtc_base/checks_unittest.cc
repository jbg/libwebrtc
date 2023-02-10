/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/checks.h"

#include "test/gtest.h"

__attribute__((__noreturn__)) int foo_fun() {
  printf("Foo\n");
  exit(1);
}

TEST(ChecksTest, NoreturnFunction) {
  printf("Noreturn in ternary branch\n");

  int dummy = (true) ? 0 : foo_fun();

  printf("%d\n", dummy);
  printf("Done\n");
}

TEST(ChecksTest, NoreturnFunction2) {
  printf("Noreturn in other ternary branch\n");

  int dummy = (false) ? foo_fun() : 0;

  printf("%d\n", dummy);
  printf("Done\n");
}

TEST(ChecksTest, WrappedInBlock) {
  printf("Wrappen in block\n");

  int dummy = 47;
  {
    dummy = (true) ? 0 : foo_fun();
  }

  printf("%d\n", dummy);
  printf("Done\n");
}

TEST(ChecksTest, NormalReturningFunction) {
  printf("Normal function\n");

  int dummy = (true) ? 0 : printf("Foo\n");

  printf("%d\n", dummy);
  printf("Done\n");
}

TEST(ChecksTest, IfElse) {
  printf("If-else\n");

  int dummy = 47;
  if ((true))
    dummy = 0;
  else
    foo_fun();

  printf("%d\n", dummy);
  printf("Done\n");
}

TEST(ChecksTest, SingleIf) {
  printf("Single if\n");

  if ((false))
    foo_fun();

  printf("Done\n");
}

TEST(ChecksTest, ExpressionNotEvaluatedWhenCheckPassing) {
  int i = 0;
  RTC_CHECK(true) << "i=" << ++i;
  RTC_CHECK_EQ(i, 0) << "Previous check passed, but i was incremented!";
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(ChecksDeathTest, Checks) {
#if RTC_CHECK_MSG_ENABLED
  EXPECT_DEATH(RTC_FATAL() << "message",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: FATAL\\(\\)\n"
               "# message");

  int a = 1, b = 2;
  EXPECT_DEATH(RTC_CHECK_EQ(a, b) << 1 << 2u,
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: a == b \\(1 vs. 2\\)\n"
               "# 12");
  RTC_CHECK_EQ(5, 5);

  RTC_CHECK(true) << "Shouldn't crash" << 1;
  EXPECT_DEATH(RTC_CHECK(false) << "Hi there!",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: false\n"
               "# Hi there!");
#else
  EXPECT_DEATH(RTC_FATAL() << "message",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");

  int a = 1, b = 2;
  EXPECT_DEATH(RTC_CHECK_EQ(a, b) << 1 << 2u,
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");
  RTC_CHECK_EQ(5, 5);

  RTC_CHECK(true) << "Shouldn't crash" << 1;
  EXPECT_DEATH(RTC_CHECK(false) << "Hi there!",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");
#endif  // RTC_CHECK_MSG_ENABLED
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
