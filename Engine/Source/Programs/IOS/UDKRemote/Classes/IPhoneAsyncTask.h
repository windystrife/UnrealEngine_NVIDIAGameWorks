// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IPhoneAppDelegate.h: IPhone application class / main loop
=============================================================================*/

#import <Foundation/Foundation.h>

@interface IPhoneAsyncTask : NSObject
{
@private
	/** Whether or not the task is ready to have GameThread callback called (set on iOS thread) */
	INT bIsReadyForGameThread;
}

/** Extra data for this async task */
@property (retain) id UserData;

/** Code to run on the game thread when the async task completes */
@property (copy) UBOOL (^GameThreadCallback)(void);

/** Run directly after GameThreadCallbackFn. This is primarily here to support code that needs to run on OS3 which can't use Blocks */
@property UBOOL (*GameThreadCallbackFn)(id UserData);

/**
 * Mark that the task is complete on the iOS thread, and now the
 * GameThread can be fired (the Task is unsafe to use after this call)
 */
- (void)FinishedTask;

/**
 * Tick all currently running tasks
 */
+ (void)TickAsyncTasks;

@end
