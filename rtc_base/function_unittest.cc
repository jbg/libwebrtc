/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "rtc_base/function.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"

namespace rtc {

namespace {

int CallWith22(rtc::Function<int(int)> f) {
  return f ? f(22) : -1;
}

int CallWith33(rtc::FunctionView<int(int)> fv) {
  return fv ? fv(33) : -1;
}

int Add33(int x) {
  return x + 33;
}

}  // namespace

// Test the main use case of Function: implicitly converting a callable
// argument.
TEST(FunctionTest, ImplicitConversion) {
  EXPECT_EQ(27, CallWith22([](int x) { return x + 5; }));
  EXPECT_EQ(55, CallWith22(Add33));
  EXPECT_EQ(-1, CallWith22(nullptr));
}

TEST(FunctionTest, IntIntLambdaWithoutState) {
  auto lam = [](int x) { return x + 1; };
  EXPECT_EQ(18, lam(17));
  rtc::Function<int(int)> f(lam);
  EXPECT_TRUE(f);
  EXPECT_EQ(18, f(17));
}

TEST(FunctionTest, IntVoidLambdaWithState) {
  int x = 13;
  auto lam = [x]() mutable { return ++x; };
  rtc::Function<int()> f(lam);
  EXPECT_TRUE(f);
  EXPECT_EQ(14, lam());
  EXPECT_EQ(15, lam());
  EXPECT_EQ(14, f());
  EXPECT_EQ(15, f());
}

TEST(FunctionTest, IntIntFunction) {
  rtc::FunctionView<int(int)> f(Add33);
  EXPECT_TRUE(f);
  EXPECT_EQ(50, f(17));
}

TEST(FunctionTest, IntIntFunctionPointer) {
  rtc::FunctionView<int(int)> f(&Add33);
  EXPECT_TRUE(f);
  EXPECT_EQ(50, f(17));
}

TEST(FunctionTest, Null) {
  // These two call constructors that statically construct null Functions.
  EXPECT_FALSE(rtc::Function<int()>());
  EXPECT_FALSE(rtc::Function<int()>(nullptr));

  // This calls the constructor for function pointers.
  EXPECT_FALSE(rtc::Function<int()>(reinterpret_cast<int (*)()>(0)));
}

TEST(FunctionTest, MoveSwapDelete) {
  struct MoveOnlyCallable {
    ~MoveOnlyCallable() { ++*y; }
    MoveOnlyCallable(MoveOnlyCallable&&) = default;
    int* operator()() { return x.get(); }
    std::unique_ptr<int> x;
    int* y;
  };

  auto x = rtc::MakeUnique<int>(5);
  int* const x_ptr = x.get();

  int num_delete_calls = 0;
  {
    MoveOnlyCallable c{std::move(x), &num_delete_calls};
    EXPECT_EQ(0, num_delete_calls);
    EXPECT_EQ(nullptr, x.get());
    EXPECT_EQ(x_ptr, c());

    {
      rtc::Function<int*()> f1 = std::move(c);
      EXPECT_EQ(0, num_delete_calls);
      EXPECT_EQ(nullptr, c());
      EXPECT_EQ(x_ptr, f1());

      {
        rtc::Function<int*()> f2 = std::move(f1);
        EXPECT_EQ(0, num_delete_calls);
        EXPECT_FALSE(f1);
        EXPECT_EQ(x_ptr, f2());

        f1 = std::move(f2);
        EXPECT_EQ(0, num_delete_calls);
        EXPECT_FALSE(f2);
        EXPECT_EQ(x_ptr, f1());

        using std::swap;
        swap(f1, f2);
        EXPECT_EQ(0, num_delete_calls);
        EXPECT_FALSE(f1);
        EXPECT_EQ(x_ptr, f2());
      }
      EXPECT_EQ(1, num_delete_calls);  // f2 expired (owned heap copy)
    }
    EXPECT_EQ(1, num_delete_calls);  // f1 expired (did not own copy)
  }
  EXPECT_EQ(2, num_delete_calls);  // c expired (was stack copy)
}

// Test the main use case of FunctionView: implicitly converting a callable
// argument.
TEST(FunctionViewTest, ImplicitConversion) {
  EXPECT_EQ(38, CallWith33([](int x) { return x + 5; }));
  EXPECT_EQ(66, CallWith33(Add33));
  EXPECT_EQ(-1, CallWith33(nullptr));
}

TEST(FunctionViewTest, IntIntLambdaWithoutState) {
  auto f = [](int x) { return x + 1; };
  EXPECT_EQ(18, f(17));
  rtc::FunctionView<int(int)> fv(f);
  EXPECT_TRUE(fv);
  EXPECT_EQ(18, fv(17));
}

TEST(FunctionViewTest, IntVoidLambdaWithState) {
  int x = 13;
  auto f = [x]() mutable { return ++x; };
  rtc::FunctionView<int()> fv(f);
  EXPECT_TRUE(fv);
  EXPECT_EQ(14, f());
  EXPECT_EQ(15, fv());
  EXPECT_EQ(16, f());
  EXPECT_EQ(17, fv());
}

TEST(FunctionViewTest, IntIntFunction) {
  rtc::FunctionView<int(int)> fv(Add33);
  EXPECT_TRUE(fv);
  EXPECT_EQ(50, fv(17));
}

TEST(FunctionViewTest, IntIntFunctionPointer) {
  rtc::FunctionView<int(int)> fv(&Add33);
  EXPECT_TRUE(fv);
  EXPECT_EQ(50, fv(17));
}

TEST(FunctionViewTest, Null) {
  // These two call constructors that statically construct null FunctionViews.
  EXPECT_FALSE(rtc::FunctionView<int()>());
  EXPECT_FALSE(rtc::FunctionView<int()>(nullptr));

  // This calls the constructor for function pointers.
  EXPECT_FALSE(rtc::FunctionView<int()>(reinterpret_cast<int (*)()>(0)));
}

// Ensure that FunctionView handles move-only arguments and return values.
TEST(FunctionViewTest, UniquePtrPassthrough) {
  auto f = [](std::unique_ptr<int> x) { return x; };
  rtc::FunctionView<std::unique_ptr<int>(std::unique_ptr<int>)> fv(f);
  std::unique_ptr<int> x(new int);
  int* x_addr = x.get();
  auto y = fv(std::move(x));
  EXPECT_EQ(x_addr, y.get());
}

TEST(FunctionViewTest, CopyConstructor) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  rtc::FunctionView<int()> fv2(fv1);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
}

TEST(FunctionViewTest, MoveConstructorIsCopy) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  rtc::FunctionView<int()> fv2(std::move(fv1));
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
}

TEST(FunctionViewTest, CopyAssignment) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  auto f23 = [] { return 23; };
  rtc::FunctionView<int()> fv2(f23);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(23, fv2());
  fv2 = fv1;
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
}

TEST(FunctionViewTest, MoveAssignmentIsCopy) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  auto f23 = [] { return 23; };
  rtc::FunctionView<int()> fv2(f23);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(23, fv2());
  fv2 = std::move(fv1);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
}

TEST(FunctionViewTest, Swap) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  auto f23 = [] { return 23; };
  rtc::FunctionView<int()> fv2(f23);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(23, fv2());
  using std::swap;
  swap(fv1, fv2);
  EXPECT_EQ(23, fv1());
  EXPECT_EQ(17, fv2());
}

// Ensure that when you copy-construct a FunctionView, the new object points to
// the same function as the old one (as opposed to the new object pointing to
// the old one).
TEST(FunctionViewTest, CopyConstructorChaining) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  rtc::FunctionView<int()> fv2(fv1);
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
  auto f23 = [] { return 23; };
  fv1 = f23;
  EXPECT_EQ(23, fv1());
  EXPECT_EQ(17, fv2());
}

// Ensure that when you assign one FunctionView to another, we actually make a
// copy (as opposed to making the second FunctionView point to the first one).
TEST(FunctionViewTest, CopyAssignmentChaining) {
  auto f17 = [] { return 17; };
  rtc::FunctionView<int()> fv1(f17);
  rtc::FunctionView<int()> fv2;
  EXPECT_TRUE(fv1);
  EXPECT_EQ(17, fv1());
  EXPECT_FALSE(fv2);
  fv2 = fv1;
  EXPECT_EQ(17, fv1());
  EXPECT_EQ(17, fv2());
  auto f23 = [] { return 23; };
  fv1 = f23;
  EXPECT_EQ(23, fv1());
  EXPECT_EQ(17, fv2());
}

}  // namespace rtc
