// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

/**
 * Implements an object that buffers text.
 *
 * The text is contiguous and, if of nonzero length, is terminated by a NULL at the very last position.
 */
class UTextBuffer
	: public UObject
	, public FOutputDevice
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UTextBuffer, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_None, COREUOBJECT_API)

public:

	/**
	 * Creates and initializes a new text buffer.
	 *
	 * @param ObjectInitializer Initialization properties.
	 * @param InText The initial text.
	 */
	COREUOBJECT_API UTextBuffer (const FObjectInitializer& ObjectInitializer, const TCHAR* InText);

	/**
	 * Creates and initializes a new text buffer.
	 *
	 * @param InText - The initial text.
	 */
	COREUOBJECT_API UTextBuffer(const TCHAR* InText);

public:

	/**
	 * Gets the buffered text.
	 *
	 * @return The text.
	 */
	const FString& GetText () const
	{
		return Text;
	}

public:

	virtual void Serialize (FArchive& Ar) override;
	virtual void Serialize (const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time ) override;

private:

	int32 Pos, Top;

	/** Holds the text. */
	FString Text;
};
