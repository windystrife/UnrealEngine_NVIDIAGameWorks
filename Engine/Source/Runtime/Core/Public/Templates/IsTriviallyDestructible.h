// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE4IsTriviallyDestructible_Private
{
	// We have this specialization for enums to avoid the need to have a full definition of
	// the type.
	template <typename T, bool bIsTriviallyTriviallyDestructible = __is_enum(T)>
	struct TImpl
	{
		enum { Value = true };
	};

	template <typename T>
	struct TImpl<T, false>
	{
		enum { Value = __has_trivial_destructor(T) };
	};
}

/**
 * Traits class which tests if a type has a trivial destructor.
 */
template <typename T>
struct TIsTriviallyDestructible
{
	enum { Value = UE4IsTriviallyDestructible_Private::TImpl<T>::Value };
};
