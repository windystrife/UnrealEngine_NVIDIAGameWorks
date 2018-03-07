// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreGlobals.h"

/**
 * This struct allows you to cache a value for a frame, and automatically invalidates
 * when the frame advances.
 * If the value was set this frame, IsSet() returns true and GetValue() is valid.
 */
template<typename ValueType>
struct TFrameValue
{
private:
	uint64 FrameSet;
	TOptional<ValueType> Value;

public:
	/** Construct an OptionaType with a valid value. */
	TFrameValue(const ValueType& InValue)
		: FrameSet(GFrameCounter)
	{
		Value = InValue;
	}

	TFrameValue(ValueType&& InValue)
		: FrameSet(GFrameCounter)
	{
		Value = InValue;
	}

	/** Construct an OptionalType with no value; i.e. unset */
	TFrameValue()
		: FrameSet(GFrameCounter)
	{
	}

	/** Copy/Move construction */
	TFrameValue(const TFrameValue& InValue)
		: FrameSet(GFrameCounter)
	{
		Value = InValue;
	}

	TFrameValue(TFrameValue&& InValue)
		: FrameSet(GFrameCounter)
	{
		Value = InValue;
	}

	TFrameValue& operator=(const TFrameValue& InValue)
	{
		Value = InValue;
		FrameSet = GFrameCounter;
		return *this;
	}

	TFrameValue& operator=(TFrameValue&& InValue)
	{
		Value = InValue;
		FrameSet = GFrameCounter;
		return *this;
	}

	TFrameValue& operator=(const ValueType& InValue)
	{
		Value = InValue;
		FrameSet = GFrameCounter;
		return *this;
	}

	TFrameValue& operator=(ValueType&& InValue)
	{
		Value = InValue;
		FrameSet = GFrameCounter;
		return *this;
	}

public:

	bool IsSet() const
	{
		return Value.IsSet() && FrameSet == GFrameCounter;
	}

	const ValueType& GetValue() const
	{
		checkf(FrameSet == GFrameCounter, TEXT("Cannot get value on a different frame"));
		return Value.GetValue();
	}
};