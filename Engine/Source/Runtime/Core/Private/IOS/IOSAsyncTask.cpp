// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSAsyncTask.h"
#include "HAL/PlatformAtomics.h"

@implementation FIOSAsyncTask

@synthesize UserData;
@synthesize GameThreadCallback;

/** All currently running tasks (which can be created on iOS thread or main thread) */
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
	self = [super init];

	// add ourself to the list of tasks
	@synchronized(RunningTasks)
	{
		[RunningTasks addObject:self];
	}

	// return ourself, the constructed object
	return self;
}


+ (void)CreateTaskWithBlock:(bool (^)(void))Block
{
	// create a task, and add it to the array
	FIOSAsyncTask* Task = [[FIOSAsyncTask alloc] init];
	// set the callback
	Task.GameThreadCallback = Block;
	// safely tell the game thread we are ready to go
	[Task FinishedTask];
}

- (void)FinishedTask
{
	FPlatformAtomics::InterlockedIncrement(&bIsReadyForGameThread);
}

/**
 * Check for completion
 *
 * @return TRUE if we succeeded (the completion block will have been called)
 */
- (bool)CheckForCompletion
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
				return true;
			}
		}
		else
		{
			// if there isn't a callback, then just return TRUE to remove the 
			// task from the queue
			return true;
		}
		
	}

	// all other cases, we are not complete
	return false;
}

/**
 * Tick all currently running tasks
 */
+ (void)ProcessAsyncTasks
{
	@synchronized(RunningTasks)
	{
		// check all outstanding tasks to see if they are done
		// (we go backwards so we can remove as we go)
		for (int32 TaskIndex = 0; TaskIndex < [RunningTasks count]; TaskIndex++)
		{
			FIOSAsyncTask* Task = [RunningTasks objectAtIndex:TaskIndex];
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