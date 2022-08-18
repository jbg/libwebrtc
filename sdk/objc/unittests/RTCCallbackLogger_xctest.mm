/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/logging/RTCCallbackLogger.h"

#import <XCTest/XCTest.h>

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "sdk/objc/unittests/xctest_to_gtest.h"
#endif

@interface RTCCallbackLoggerTests : XCTestCase

@property(nonatomic, strong) RTC_OBJC_TYPE(RTCCallbackLogger) * logger;

@end

@implementation RTCCallbackLoggerTests

@synthesize logger;

- (void)setUp {
  self.logger = [[RTC_OBJC_TYPE(RTCCallbackLogger) alloc] init];
}

- (void)tearDown {
  self.logger = nil;
}

- (void)testDefaultSeverityLevel {
  XCTAssertEqual(self.logger.severity, RTCLoggingSeverityInfo);
}

- (void)testCallbackGetsCalledForAppropriateLevel {
  self.logger.severity = RTCLoggingSeverityWarning;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackWarning"];

  [self.logger start:^(NSString *message) {
    XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
    [callbackExpectation fulfill];
  }];

  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testCallbackWithSeverityGetsCalledForAppropriateLevel {
  self.logger.severity = RTCLoggingSeverityWarning;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackWarning"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity severity) {
        XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
        XCTAssertEqual(severity, RTCLoggingSeverityError);
        [callbackExpectation fulfill];
      }];

  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testCallbackDoesNotGetCalledForOtherLevels {
  self.logger.severity = RTCLoggingSeverityError;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackError"];

  [self.logger start:^(NSString *message) {
    XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
    [callbackExpectation fulfill];
  }];

  RTCLogInfo("Just some info");
  RTCLogWarning("Warning warning");
  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testCallbackWithSeverityDoesNotGetCalledForOtherLevels {
  self.logger.severity = RTCLoggingSeverityError;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackError"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity severity) {
        XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
        XCTAssertEqual(severity, RTCLoggingSeverityError);
        [callbackExpectation fulfill];
      }];

  RTCLogInfo("Just some info");
  RTCLogWarning("Warning warning");
  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testCallbackDoesNotgetCalledForSeverityNone {
  self.logger.severity = RTCLoggingSeverityNone;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"unexpectedCallback"];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
    XCTAssertTrue(false);
  }];

  RTCLogInfo("Just some info");
  RTCLogWarning("Warning warning");
  RTCLogError("Horrible error");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testCallbackWithSeverityDoesNotgetCalledForSeverityNone {
  self.logger.severity = RTCLoggingSeverityNone;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"unexpectedCallback"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity severity) {
        [callbackExpectation fulfill];
        XCTAssertTrue(false);
      }];

  RTCLogInfo("Just some info");
  RTCLogWarning("Warning warning");
  RTCLogError("Horrible error");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testStartingWithNilCallbackDoesNotCrash {
  [self.logger start:nil];

  RTCLogError("Horrible error");
}

- (void)testStartingWithNilCallbackWithSeverityDoesNotCrash {
  [self.logger startWithMessageAndSeverityHandler:nil];

  RTCLogError("Horrible error");
}

- (void)testStopCallbackLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"stopped"];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
  }];

  [self.logger stop];

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testStopCallbackWithSeverityLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"stopped"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity loggingServerity) {
        [callbackExpectation fulfill];
      }];

  [self.logger stop];

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testDestroyingCallbackLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"destroyed"];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
  }];

  self.logger = nil;

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testDestroyingCallbackWithSeverityLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"destroyed"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity loggingServerity) {
        [callbackExpectation fulfill];
      }];

  self.logger = nil;

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testCallbackWithSeverityLoggerCannotStartTwice {
  self.logger.severity = RTCLoggingSeverityWarning;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackWarning"];

  [self.logger
      startWithMessageAndSeverityHandler:^(NSString *message, RTCLoggingSeverity loggingServerity) {
        XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
        XCTAssertEqual(loggingServerity, RTCLoggingSeverityError);
        [callbackExpectation fulfill];
      }];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
    XCTAssertTrue(false);
  }];

  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

@end

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

class RTCCallbackLoggerTest : public XCTestToGTest<RTCCallbackLoggerTests> {
 public:
  RTCCallbackLoggerTest() = default;
  ~RTCCallbackLoggerTest() override = default;
};

INVOKE_XCTEST(RTCCallbackLoggerTest, DefaultSeverityLevel)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackGetsCalledForAppropriateLevel)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackWithSeverityGetsCalledForAppropriateLevel)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackDoesNotGetCalledForOtherLevels)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackWithSeverityDoesNotGetCalledForOtherLevels)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackDoesNotgetCalledForSeverityNone)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackWithSeverityDoesNotgetCalledForSeverityNone)

INVOKE_XCTEST(RTCCallbackLoggerTest, StartingWithNilCallbackDoesNotCrash)

INVOKE_XCTEST(RTCCallbackLoggerTest, StartingWithNilCallbackWithSeverityDoesNotCrash)

INVOKE_XCTEST(RTCCallbackLoggerTest, StopCallbackLogger)

INVOKE_XCTEST(RTCCallbackLoggerTest, StopCallbackWithSeverityLogger)

INVOKE_XCTEST(RTCCallbackLoggerTest, DestroyingCallbackLogger)

INVOKE_XCTEST(RTCCallbackLoggerTest, DestroyingCallbackWithSeverityLogger)

INVOKE_XCTEST(RTCCallbackLoggerTest, CallbackWithSeverityLoggerCannotStartTwice)

}  // namespace webrtc

#endif
