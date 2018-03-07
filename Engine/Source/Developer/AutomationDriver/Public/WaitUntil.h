// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IElementLocator;

/**
 * Represents the state of an active wait action for the driver
 */
struct AUTOMATIONDRIVER_API FDriverWaitResponse
{
public:

	enum class EState : uint8
	{
		PASSED,
		WAIT,
		FAILED,
	};

	/**
	 * @return a FDriverWaitResponse with a state of PASSED and a wait of zero
	 */
	static FDriverWaitResponse Passed();

	/**
	 * @return a FDriverWaitResponse with a state of WAIT and a wait of 0.5 seconds
	 */
	static FDriverWaitResponse Wait();

	/**
	 * @return a FDriverWaitResponse with a state of WAIT and a wait of the specified timespan
	 */
	static FDriverWaitResponse Wait(FTimespan Timespan);

	/**
	 * @return a FDriverWaitResponse with a state of FAILED and a wait of zero
	 */
	static FDriverWaitResponse Failed();

	FDriverWaitResponse(EState InState, FTimespan InNextWait);

	// How long the driver should wait before re-evaluating the wait condition again
	const FTimespan NextWait;

	// Whether the wait condition is completely finished or should be rescheduled again for execution
	const EState State;
};

DECLARE_DELEGATE_RetVal_OneParam(FDriverWaitResponse, FDriverWaitDelegate, const FTimespan& /*TotalWaitTime*/);
DECLARE_DELEGATE_RetVal(bool, FDriverWaitConditionDelegate);

/**
 * A fluent wrapper around timespan to enforce obvious differences between specified Timeout and Interval values for waits
 */
class AUTOMATIONDRIVER_API FWaitTimeout
{
public:

	static FWaitTimeout InMilliseconds(double Value);
	static FWaitTimeout InSeconds(double Value);
	static FWaitTimeout InMinutes(double Value);
	static FWaitTimeout InHours(double Value);

	explicit FWaitTimeout(FTimespan InTimespan);

	const FTimespan Timespan;
};

/**
 * A fluent wrapper around timespan to enforce obvious differences between specified Timeout and Interval values for waits
 */
class AUTOMATIONDRIVER_API FWaitInterval
{
public:

	static FWaitInterval InMilliseconds(double Value);
	static FWaitInterval InSeconds(double Value);
	static FWaitInterval InMinutes(double Value);
	static FWaitInterval InHours(double Value);

	explicit FWaitInterval(FTimespan InTimespan);

	const FTimespan Timespan;
};

/**
 * Represents a collection of fluent helper functions designed to make accessing and creating driver wait delegates easier
 */
class AUTOMATIONDRIVER_API Until
{
public:

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers elements or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate ElementExists(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers elements or if the specified timeout timespan elapses;
	 * The element locator is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate ElementExists(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers visible elements or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate ElementIsVisible(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers visible elements or if the specified timeout timespan elapses;
	 * The element locator is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate ElementIsVisible(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers interactable elements or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate ElementIsInteractable(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers interactable elements or if the specified timeout timespan elapses;
	 * The element locator is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate ElementIsInteractable(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers a scrollable element whose scroll position is at the beginning
	 * or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate ElementIsScrolledToBeginning(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers a scrollable element whose scroll position is at the beginning
	 * or if the specified timeout timespan elapses
	 * The element locator is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate ElementIsScrolledToBeginning(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers a scrollable element whose scroll position is at the end
	 * or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate ElementIsScrolledToEnd(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitTimeout Timeout);
	
	/**
	 * Creates a new wait delegate which completes it's wait only if the specified element locator discovers a scrollable element whose scroll position is at the end
	 * or if the specified timeout timespan elapses
	 * The element locator is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate ElementIsScrolledToEnd(const TSharedRef<IElementLocator, ESPMode::ThreadSafe>& ElementLocator, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified condition returns true or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate Condition(const TFunction<bool()>& Function, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified condition returns true or if the specified timeout timespan elapses
	 * The lambda is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate Condition(const TFunction<bool()>& Function, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified condition returns true or if the specified timeout timespan elapses
	 */
	static FDriverWaitDelegate Condition(const FDriverWaitConditionDelegate& Delegate, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which completes it's wait only if the specified condition returns true or if the specified timeout timespan elapses
	 * The delegate is only re-evaluated at the specified wait interval
	 */
	static FDriverWaitDelegate Condition(const FDriverWaitConditionDelegate& Delegate, FWaitInterval Interval, FWaitTimeout Timeout);

	/**
	 * Creates a new wait delegate which drives its state off the result of the specified lambda
	 */
	static FDriverWaitDelegate Lambda(const TFunction<FDriverWaitResponse(const FTimespan&)>& Value);
};
