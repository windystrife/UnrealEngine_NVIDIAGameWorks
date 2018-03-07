// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/* Custom run-loop modes for UE4 that process only certain kinds of events to simulate Windows event ordering. */
extern NSString* UE4NilEventMode; /* Process only mandatory events */
extern NSString* UE4ShowEventMode; /* Process only show window events */
extern NSString* UE4ResizeEventMode; /* Process only resize/move window events */
extern NSString* UE4FullscreenEventMode; /* Process only fullscreen mode events */
extern NSString* UE4CloseEventMode; /* Process only close window events */
extern NSString* UE4IMEEventMode; /* Process only input method events */

@interface NSThread (FCocoaThread)
+ (NSThread*) gameThread; // Returns the main game thread, or nil if has yet to be constructed.
+ (bool) isGameThread; // True if the current thread is the main game thread, else false.
- (bool) isGameThread; // True if this thread object is the main game thread, else false.
@end

@interface FCocoaGameThread : NSThread
- (id)init; // Override that sets the variable backing +[NSThread gameThread], do not override in a subclass.
- (id)initWithTarget:(id)Target selector:(SEL)Selector object:(id)Argument; // Override that sets the variable backing +[NSThread gameThread], do not override in a subclass.
- (void)dealloc; // Override that clears the variable backing +[NSThread gameThread], do not override in a subclass.
- (void)main; // Override that sets the variable backing +[NSRunLoop gameRunLoop], do not override in a subclass.
@end

void MainThreadCall(dispatch_block_t Block, NSString* WaitMode = NSDefaultRunLoopMode, bool const bWait = true);

template<typename ReturnType>
ReturnType MainThreadReturn(ReturnType (^Block)(void), NSString* WaitMode = NSDefaultRunLoopMode)
{
	__block ReturnType ReturnValue;
	MainThreadCall(^{ ReturnValue = Block(); }, WaitMode, true);
	return ReturnValue;
}

void GameThreadCall(dispatch_block_t Block, NSArray* SendModes = @[ NSDefaultRunLoopMode ], bool const bWait = true);

template<typename ReturnType>
ReturnType GameThreadReturn(ReturnType (^Block)(void), NSArray* SendModes = @[ NSDefaultRunLoopMode ])
{
	__block ReturnType ReturnValue;
	GameThreadCall(^{ ReturnValue = Block(); }, SendModes, true);
	return ReturnValue;
}

void RunGameThread(id Target, SEL Selector);

void ProcessGameThreadEvents(void);
