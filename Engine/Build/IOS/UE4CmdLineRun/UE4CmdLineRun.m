// Copyright 2013-2017 Epic Games, Inc. All Rights Reserved.

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface UE4CmdLineRun : XCTestCase

@end

@implementation UE4CmdLineRun

- (void)setUp
{
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testCmdLineRun
{
	// by running the runloop from the test, the full game will be running as if launched normalled
	// the unit test is run from the startup of the normal application path, this lets it continue on
	// since the system will quit the app as soon as all tests are complete
	[[NSRunLoop currentRunLoop] run];
}

@end
