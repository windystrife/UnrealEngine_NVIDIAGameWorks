// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * A handle to a throttle request made to the throttle manager
 * Throttles can only be ended by passing back a request
 */
class FThrottleRequest
{
	// Only the manager is allowed to change values
	friend class FSlateThrottleManager;

public:

	FThrottleRequest()
		: Index(INDEX_NONE)
	{ }

public:

	/**
	 * Whether or not the handle is valid
	 *
	 * @return true if the throttle is valid
	 */
	bool IsValid() const
	{
		return Index != INDEX_NONE;
	}

private:

	int32 Index;
};


/**
 * A class which manages requests to throttle parts of the engine to ensure Slate UI performance                   
 */
class SLATECORE_API FSlateThrottleManager
{
public:

	/**
	 * Constructor                   
	 */
	FSlateThrottleManager();

public:

	/**
	 * Requests that we enter responsive mode.  I.E throttle slow parts of the engine   
	 *
	 * @return A handle to the request to enter responsive mode.  Can only be ended with this request
	 */
	FThrottleRequest EnterResponsiveMode();

	/**
	 * Request to leave responsive mode.  
	 * Note that this may not end responsive mode in the case that multiple EnterResponsiveMode requests were made                    
	 *
	 * @param InHandle	The handle that was created with EnterResponsiveMode
	 */
	void LeaveResponsiveMode( FThrottleRequest& InHandle );

	/**
	 * Whether or not we allow expensive tasks which could hurt performance to occur
	 *
	 * @return true if we allow expensive tasks, false otherwise
	 */
	bool IsAllowingExpensiveTasks() const;

public:

	/**
	 * Gets the instance of this manager                   
	 */
	static FSlateThrottleManager& Get( );

private:
	/** CVar variable to check if we allow throttling (int32 for compatibility) */
	int32 bShouldThrottle;

	/** CVar allowing us to toggle throttling ability */
	FAutoConsoleVariableRef CVarAllowThrottle;

	/** Number of active throttle requests */
	uint32 ThrottleCount;
};
