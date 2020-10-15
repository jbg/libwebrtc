/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capturer_win_wgc.h"

#include <tchar.h>
#include <winbase.h>
#include <windef.h>
#include <windows.h>
#include <winuser.h>
#include <string>
#include <utility>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/win/scoped_com_initializer.h"
#include "rtc_base/win/windows_version.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {

const wchar_t kWindowClass[] = L"TestWindowClass";
const wchar_t kWindowTitle[] = L"Test Window";
const char kWindowTitleString[] = "Test Window";

const int kSmallWindowWidth = 200;
const int kSmallWindowHeight = 100;
const int kWindowWidth = 300;
const int kWindowHeight = 200;
const int kLargeWindowWidth = 400;
const int kLargeWindowHeight = 300;

// The size of the image we capture is slightly smaller than the actual size of
// the window.
const int kWindowWidthSubtrahend = 14;
const int kWindowHeightSubtrahend = 7;

namespace {

// Message loop for the test window that we create to test capturing.
LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0L;
  }

  return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace

class WindowCapturerWinWgcTest : public ::testing::Test,
                                 public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    com_initializer_ = std::make_unique<rtc_base::win::ScopedCOMInitializer>(
        rtc_base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(com_initializer_->Succeeded());
    capturer_ = WindowCapturerWinWgc::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    ASSERT_TRUE(capturer_);
  }

  void TearDown() override {
    if (window_open_) {
      DestroyTestWindow();
    }
    EXPECT_TRUE(
        ::UnregisterClass(MAKEINTATOM(window_class_), window_instance_));
  }

  void CreateTestWindow() {
    RTC_DCHECK(!window_open_);
    EXPECT_TRUE(::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&WndProc), &window_instance_));

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance = window_instance_;
    wcex.lpfnWndProc = &WndProc;
    wcex.lpszClassName = kWindowClass;
    window_class_ = ::RegisterClassEx(&wcex);
    EXPECT_TRUE(window_class_);

    hwnd_ = ::CreateWindow(
        kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, kWindowWidth, kWindowHeight, /*parent_window=*/nullptr,
        /*menu_bar=*/nullptr, window_instance_, /*additional_params=*/nullptr);
    EXPECT_TRUE(hwnd_);

    ::ShowWindow(hwnd_, SW_SHOW);
    ::UpdateWindow(hwnd_);
    window_open_ = true;

    RECT window_rect;
    ::GetWindowRect(hwnd_, &window_rect);
    EXPECT_EQ(window_rect.right - window_rect.left, kWindowWidth);
    EXPECT_EQ(window_rect.bottom - window_rect.top, kWindowHeight);

    // Give the window a chance to fully open before we try to capture it.
    SleepMs(200);
  }

  void ResizeTestWindow(const int width, const int height) {
    RTC_DCHECK(window_open_);
    EXPECT_TRUE(::SetWindowPos(hwnd_, HWND_TOP, /*x-coord=*/0, /*y-coord=*/0,
                               width, height, SWP_SHOWWINDOW));
    EXPECT_TRUE(::UpdateWindow(hwnd_));
  }

  void DestroyTestWindow() {
    RTC_DCHECK(window_open_);
    EXPECT_TRUE(::DestroyWindow(hwnd_));
    window_open_ = false;
  }

  DesktopCapturer::SourceId FindTestWindowId() {
    DesktopCapturer::SourceList sources;
    EXPECT_TRUE(capturer_->GetSourceList(&sources));

    auto it =
        std::find_if(sources.begin(), sources.end(),
                     [&](const DesktopCapturer::Source& src) {
                       return src.id == reinterpret_cast<intptr_t>(hwnd_) &&
                              src.title == kWindowTitleString;
                     });

    EXPECT_NE(it, sources.end());
    return it->id;
  }

  void DoCapture() {
    capturer_->CaptureFrame();

    // Sometimes the first few frames are empty becaues the capture engine is
    // still starting up. We also may drop a few frames when the window is
    // resized or un-minimized.
    while (result_ == DesktopCapturer::Result::ERROR_TEMPORARY) {
      capturer_->CaptureFrame();
    }

    ASSERT_EQ(result_, DesktopCapturer::Result::SUCCESS);
    ASSERT_TRUE(frame_);
  }

  // DesktopCapturer::Callback interface
  // The capturer synchronously invokes this method before |CaptureFrame()|
  // returns.
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    result_ = result;
    frame_ = std::move(frame);
  }

 protected:
  std::unique_ptr<rtc_base::win::ScopedCOMInitializer> com_initializer_;
  std::unique_ptr<DesktopCapturer> capturer_;
  std::unique_ptr<DesktopFrame> frame_;
  DesktopCapturer::Result result_;
  HINSTANCE window_instance_;
  ATOM window_class_;
  HWND hwnd_ = nullptr;
  bool window_open_ = false;
};

TEST_F(WindowCapturerWinWgcTest, SourceSelection) {
  DesktopCapturer::Source invalid_src;
  EXPECT_FALSE(capturer_->SelectSource(invalid_src.id));

  invalid_src.id = 0x0000;
  EXPECT_FALSE(capturer_->SelectSource(invalid_src.id));

  CreateTestWindow();
  DesktopCapturer::SourceId src_id = FindTestWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  // Minimize the window.
  EXPECT_TRUE(::CloseWindow(hwnd_));
  EXPECT_FALSE(capturer_->SelectSource(src_id));

  // Reopen the window.
  EXPECT_TRUE(::OpenIcon(hwnd_));
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  // Close the window.
  DestroyTestWindow();
  EXPECT_FALSE(capturer_->SelectSource(src_id));
}

TEST_F(WindowCapturerWinWgcTest, Capture) {
  if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
    RTC_LOG(LS_INFO) << "Skipping test on Windows versions < RS5.";
    GTEST_SKIP();
  }

  CreateTestWindow();
  DesktopCapturer::SourceId src_id = FindTestWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  RTC_LOG(LS_INFO) << "Did capture.";
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);
}

TEST_F(WindowCapturerWinWgcTest, ResizeWindowMidCapture) {
  if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
    RTC_LOG(LS_INFO) << "Skipping test on Windows versions < RS5.";
    GTEST_SKIP();
  }

  CreateTestWindow();
  DesktopCapturer::SourceId src_id = FindTestWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(kLargeWindowWidth, kLargeWindowHeight);
  DoCapture();
  // We don't expect to see the new size until the next capture.
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kLargeWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kLargeWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(kSmallWindowWidth, kSmallWindowHeight);
  DoCapture();
  // We don't expect to see the new size until the next capture.
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kSmallWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kSmallWindowHeight - kWindowHeightSubtrahend);

  // Minmize the window and capture should continue but return temporary errors.
  EXPECT_TRUE(::CloseWindow(hwnd_));
  for (int i = 0; i < 10; ++i) {
    capturer_->CaptureFrame();
    EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_TEMPORARY);
  }

  // Reopen the window and the capture should continue normally.
  EXPECT_TRUE(::OpenIcon(hwnd_));
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kSmallWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kSmallWindowHeight - kWindowHeightSubtrahend);
}

TEST_F(WindowCapturerWinWgcTest, CloseWindowMidCapture) {
  if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
    RTC_LOG(LS_INFO) << "Skipping test on Windows versions < RS5.";
    GTEST_SKIP();
  }

  CreateTestWindow();
  DesktopCapturer::SourceId src_id = FindTestWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  DestroyTestWindow();
  // The window may not close immediately, so we may get a few frames before it
  // closes, and also a couple dropped frames between the time the window
  // disappears and the capturer receives the Closed event and stops capturing.
  while (result_ == DesktopCapturer::Result::SUCCESS ||
         result_ == DesktopCapturer::Result::ERROR_TEMPORARY)
    capturer_->CaptureFrame();
  EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_PERMANENT);
}

}  // namespace webrtc
