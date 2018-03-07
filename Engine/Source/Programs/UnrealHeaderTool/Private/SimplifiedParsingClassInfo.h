// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FArchive;

/**
 * Simplified parsing information about UClasses.
 */
class FSimplifiedParsingClassInfo
{
public:
	// Constructor.
	FSimplifiedParsingClassInfo(FString InClassName, FString InBaseClassName, int32 InClassDefLine, bool bInClassIsAnInterface)
		: ClassName          (MoveTemp(InClassName))
		, BaseClassName      (MoveTemp(InBaseClassName))
		, ClassDefLine       (InClassDefLine)
		, bClassIsAnInterface(bInClassIsAnInterface)
	{}

	FSimplifiedParsingClassInfo() { }

	/**
	 * Gets class name.
	 */
	const FString& GetClassName() const
	{
		return ClassName;
	}

	/**
	 * Gets base class name.
	 */
	const FString& GetBaseClassName() const
	{
		return BaseClassName;
	}

	/**
	 * Gets class definition line number.
	 */
	int32 GetClassDefLine() const
	{
		return ClassDefLine;
	}

	/**
	 * Tells if this class is an interface.
	 */
	bool IsInterface() const
	{
		return bClassIsAnInterface;
	}

	friend FArchive& operator<<(FArchive& Ar, FSimplifiedParsingClassInfo& SimplifiedParsingClassInfo)
	{
		Ar << SimplifiedParsingClassInfo.ClassName;
		Ar << SimplifiedParsingClassInfo.BaseClassName;
		Ar << SimplifiedParsingClassInfo.ClassDefLine;
		Ar << SimplifiedParsingClassInfo.bClassIsAnInterface;

		return Ar;
	}

private:
	// Name.
	FString ClassName;

	// Super-class name.
	FString BaseClassName;

	// Class definition line.
	int32 ClassDefLine;

	// Is this an interface class?
	bool bClassIsAnInterface;
};
