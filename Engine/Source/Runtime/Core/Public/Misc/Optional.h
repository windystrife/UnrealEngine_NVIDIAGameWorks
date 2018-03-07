// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"

/**
 * When we have an optional value IsSet() returns true, and GetValue() is meaningful.
 * Otherwise GetValue() is not meaningful.
 */
template<typename OptionalType>
struct TOptional
{
public:
	/** Construct an OptionaType with a valid value. */
	TOptional(const OptionalType& InValue)
	{
		new(&Value) OptionalType(InValue);
		bIsSet = true;
	}
	TOptional(OptionalType&& InValue)
	{
		new(&Value) OptionalType(MoveTempIfPossible(InValue));
		bIsSet = true;
	}

	/** Construct an OptionalType with no value; i.e. unset */
	TOptional()
		: bIsSet(false)
	{
	}

	~TOptional()
	{
		Reset();
	}

	/** Copy/Move construction */
	TOptional(const TOptional& InValue)
		: bIsSet(false)
	{
		if (InValue.bIsSet)
		{
			new(&Value) OptionalType(*(const OptionalType*)&InValue.Value);
			bIsSet = true;
		}
	}
	TOptional(TOptional&& InValue)
		: bIsSet(false)
	{
		if (InValue.bIsSet)
		{
			new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&InValue.Value));
			bIsSet = true;
		}
	}

	TOptional& operator=(const TOptional& InValue)
	{
		if (&InValue != this)
		{
			Reset();
			if (InValue.bIsSet)
			{
				new(&Value) OptionalType(*(const OptionalType*)&InValue.Value);
				bIsSet = true;
			}
		}
		return *this;
	}
	TOptional& operator=(TOptional&& InValue)
	{
		if (&InValue != this)
		{
			Reset();
			if (InValue.bIsSet)
			{
				new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&InValue.Value));
				bIsSet = true;
			}
		}
		return *this;
	}

	TOptional& operator=(const OptionalType& InValue)
	{
		if (&InValue != (OptionalType*)&Value)
		{
			Reset();
			new(&Value) OptionalType(InValue);
			bIsSet = true;
		}
		return *this;
	}
	TOptional& operator=(OptionalType&& InValue)
	{
		if (&InValue != (OptionalType*)&Value)
		{
			Reset();
			new(&Value) OptionalType(MoveTempIfPossible(InValue));
			bIsSet = true;
		}
		return *this;
	}

	void Reset()
	{
		if (bIsSet)
		{
			bIsSet = false;

			// We need a typedef here because VC won't compile the destructor call below if OptionalType itself has a member called OptionalType
			typedef OptionalType OptionalDestructOptionalType;
			((OptionalType*)&Value)->OptionalDestructOptionalType::~OptionalDestructOptionalType();
		}
	}

	template <typename... ArgsType>
	void Emplace(ArgsType&&... Args)
	{
		Reset();
		new(&Value) OptionalType(Forward<ArgsType>(Args)...);
		bIsSet = true;
	}

	friend bool operator==(const TOptional& lhs, const TOptional& rhs)
	{
		if (lhs.bIsSet != rhs.bIsSet)
		{
			return false;
		}
		if (!lhs.bIsSet) // both unset
		{
			return true;
		}
		return (*(OptionalType*)&lhs.Value) == (*(OptionalType*)&rhs.Value);
	}
	friend bool operator!=(const TOptional& lhs, const TOptional& rhs)
	{
		return !(lhs == rhs);
	}

	/** @return true when the value is meaningful; false if calling GetValue() is undefined. */
	bool IsSet() const { return bIsSet; }
	FORCEINLINE explicit operator bool() const { return bIsSet; }

	/** @return The optional value; undefined when IsSet() returns false. */
	const OptionalType& GetValue() const { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return *(OptionalType*)&Value; }
	      OptionalType& GetValue()       { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return *(OptionalType*)&Value; }

	const OptionalType* operator->() const { return &GetValue(); }
	      OptionalType* operator->()       { return &GetValue(); }

	/** @return The optional value when set; DefaultValue otherwise. */
	const OptionalType& Get(const OptionalType& DefaultValue) const { return IsSet() ? *(OptionalType*)&Value : DefaultValue; }

private:
	TTypeCompatibleBytes<OptionalType> Value;
	bool bIsSet;
};
