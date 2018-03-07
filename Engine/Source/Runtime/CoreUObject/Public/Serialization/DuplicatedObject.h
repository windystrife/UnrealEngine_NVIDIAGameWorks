// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Information about a duplicated object
 * For use with a dense object annotation
 */
struct FDuplicatedObject
{
	/** The duplicated object */
	UObject* DuplicatedObject;

	FDuplicatedObject()
		: DuplicatedObject(NULL)
	{

	}

	FDuplicatedObject( UObject* InDuplicatedObject )
		: DuplicatedObject( InDuplicatedObject )
	{

	}

	/**
	 * @return true if this is the default annotation and holds no information about a duplicated object
	 */
	FORCEINLINE bool IsDefault()
	{
		return DuplicatedObject == NULL;
	}
};

template <> struct TIsPODType<FDuplicatedObject> { enum { Value = true }; };

