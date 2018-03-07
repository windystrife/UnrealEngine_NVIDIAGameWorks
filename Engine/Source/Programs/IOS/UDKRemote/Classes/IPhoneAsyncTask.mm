// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EAGLView.mm: IPhone window wrapper for a GL view
=============================================================================*/

#include "Engine.h"
#import "IPhoneAsyncTask.h"

@implementation IPhoneAsyncTask

@synthesize UserData;
@synthesize GameThreadCallback;
@synthesize GameThreadCallbackFn;

/** All currently running tasks (can be created on iOS thread or main thread) */
NSMutableArray* RunningTasks;

/**
 * Static class constructor, called before any instances are created
 */
+ (void)initialize
{
	// create the RunningTasks object one time
	RunningTasks = [[NSMutableArray arrayWithCapacity:4] retain];
}

/**
 * Initialize the async task
 */
- (id)init
{
	// add ourself to the list of tasks
	@synchronized(RunningTasks)
	{
		[RunningTasks addObject:self];
	}

	// return ourself, the constructed object
	return self;
}

/**
 * Mark that the task is complete on the iOS thread, and now the
 * GameThread can be fired (the Task is unsafe to use after this call)
 */
- (void)FinishedTask
{
	appInterlockedIncrement(&bIsReadyForGameThread);
}

/**
 * Check for completion
 *
 * @return TRUE if we succeeded (the completion block will have been called)
 */
- (UBOOL)CheckForCompletion
{
	// handle completion
	if (bIsReadyForGameThread)
	{
		// call the game thread block
		if (GameThreadCallback)
		{
			if (GameThreadCallback())
			{
				// only return true if the callback says it's complete
				return TRUE;
			}
		}
		else if (GameThreadCallbackFn)
		{
			if (GameThreadCallbackFn(UserData))
			{
				// only return true if the callback says it's complete
				return TRUE;
			}
		}
		else
		{
			// there there is neither type of callback, then just return TRUE to remove the 
			// task from the queue
			return TRUE;
		}
		
	}

	// all other cases, we are not complete
	return FALSE;
}

/**
 * Tick all currently running tasks
 */
+ (void)TickAsyncTasks
{
	@synchronized(RunningTasks)
	{
		// check all outstanding tasks to see if they are done
		// (we go backwards so we can remove as we go)
		for (INT TaskIndex = 0; TaskIndex < [RunningTasks count]; TaskIndex++)
		{
			IPhoneAsyncTask* Task = [RunningTasks objectAtIndex:TaskIndex];
			// if it finished, remove it from the list of running tasks
			if ([Task CheckForCompletion])
			{
				// release the object count
				[Task release];

				// and remove from the list
				[RunningTasks removeObjectAtIndex:TaskIndex];
				TaskIndex--;
			}
		}
	}
}


/** 
 * Application destructor
 */
- (void)dealloc 
{
	[GameThreadCallback release];
	self.UserData = nil;

	[super dealloc];
}


@end