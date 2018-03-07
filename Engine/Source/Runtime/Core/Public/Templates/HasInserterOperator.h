// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE4HasInserterOperator_Private
{
	struct FNotSpecified {};

	template <typename T>
	struct FReturnValueCheck
	{
		static char (&Func())[2];
	};

	template <>
	struct FReturnValueCheck<FNotSpecified>
	{
		static char (&Func())[1];
	};

	template <typename Dest, typename T>
	FNotSpecified operator<<(Dest&&, T&&);

	template <typename T>
	T&& Make();

	template <typename Dest, typename T, typename = decltype(Make<Dest&>() << Make<T&>())>
	struct THasInserterOperator
	{
		enum { Value = true };
	};

	template <typename Dest, typename T>
	struct THasInserterOperator<Dest, T, FNotSpecified>
	{
		enum { Value = false };
	};
}

/**
 * Traits class which tests if a type has an operator<< overload between two types.
 */
template <typename Dest, typename T>
struct THasInserterOperator
{
	enum { Value = UE4HasInserterOperator_Private::THasInserterOperator<Dest, T>::Value };
};
