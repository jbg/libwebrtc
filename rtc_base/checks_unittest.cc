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

#define RTC_HELLO()   \
  do {                \
    printf("Hello "); \
  } while (0)

TEST(ChecksTest, RandomMacro) {
  RTC_HELLO();
  printf("World\n");
}

TEST(ChecksTest, MultipleChecks) {
  printf("Lets go!\n");

  RTC_CHECK(true);
  RTC_DCHECK(true);
  RTC_CHECK(true);
  int chars_printed = 0;
  chars_printed = printf("Woohoo!\n");

  RTC_CHECK(chars_printed > 0);
  RTC_CHECK(chars_printed > 0);
  printf("Hooray!\n");
  printf("Done\n");
}

TEST(ChecksTest, MultipleDchecks) {
  printf("Same for DCHECK!\n");

  RTC_DCHECK(true);
  RTC_DCHECK(true);
  RTC_DCHECK(true);
  int chars_printed = 0;
  chars_printed = printf("Yay!\n");

  RTC_DCHECK(chars_printed > 0);
  RTC_DCHECK(chars_printed > 0);
  printf("Yippee!\n");
  printf("Done\n");
}

TEST(ChecksTest, ExplicitReplacement) {
  printf("Explicit!\n");

  (true) ? static_cast<void>(0)
         : ::rtc::webrtc_checks_impl::FatalLogCall<false>(__FILE__, __LINE__,
                                                          "true") &
               ::rtc::webrtc_checks_impl::LogStreamer<>();

  int chars_printed = 0;
  chars_printed = printf("Foo!\n");
  printf("%d\n", chars_printed);
}

TEST(ChecksTest, ExplicitReplacement2) {
  printf("Explicit2!\n");

  (true)
      ?
      static_cast<void>(0)
      :
      ::rtc::webrtc_checks_impl::FatalLogCall<false>(__FILE__, __LINE__,"true")
      & ::rtc::webrtc_checks_impl::LogStreamer<>();

  int chars_printed = 0;
  chars_printed = printf("Bar!\n");
  printf("%d\n", chars_printed);
}

TEST(ChecksTest, ExplicitReplacement3) {
  printf("Explicit3!\n");

  int dummy = (true) ? 0
      : printf("FatalLogCall<false>\n") &
      printf("LogStreamer\n");

  printf("%d\n", dummy);
}


TEST(ChecksTest, ExplicitReplacement4) {
  printf("Explicit4!\n");

  auto a =  ::rtc::webrtc_checks_impl::FatalLogCall<false>(__FILE__, __LINE__,
                                                           "true");
  auto b =  ::rtc::webrtc_checks_impl::LogStreamer<>();

  (true) ? static_cast<void>(0)
         : a &
         b;

  int chars_printed = 0;
  chars_printed = printf("Foo!\n");
  printf("%d\n", chars_printed);
}


class Bar {
 public:
  void call() const {
    printf("Foo & Bar\n");
    exit(1);
  }
};

class Foo {
 public:
  void operator&(const Bar& b) {
    b.call();
  }
};

TEST(ChecksTest, ExplicitReplacement5) {
  printf("Explicit5!\n");

  Foo a;
  Bar b;

  (true) ? static_cast<void>(0)
         : a &
         b;

  int chars_printed = 0;
  chars_printed = printf("FooBar!\n");
  printf("%d\n", chars_printed);
}




TEST(ChecksTest, CheckEq) {
  int i = 47;
  RTC_CHECK_EQ(i, 47);
  printf("Still there?\n");

  RTC_CHECK_EQ(i, 47) << "Whoopee";
  printf("Yes!\n");
}

TEST(ChecksTest, ExpressionNotEvaluatedWhenCheckPassing) {
  int i = 0;
  RTC_CHECK(true) << "i=" << ++i;
  (void)i;
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
