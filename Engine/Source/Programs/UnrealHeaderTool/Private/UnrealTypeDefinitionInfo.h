// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

// Forward declarations.
class FUnrealSourceFile;

/**
 * Class that stores information about type (USTRUCT/UCLASS) definition.
 */
class FUnrealTypeDefinitionInfo : public TSharedFromThis<FUnrealTypeDefinitionInfo>
{
public:
	// Constructor
	FUnrealTypeDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber)
		: SourceFile(InSourceFile)
		, LineNumber(InLineNumber)
	{ }

	/**
	 * Gets the line number in source file this type was defined in.
	 */
	int32 GetLineNumber() const
	{
		return LineNumber;
	}

	/**
	 * Gets the reference to FUnrealSourceFile object that stores information about
	 * source file this type was defined in.
	 */
	FUnrealSourceFile& GetUnrealSourceFile()
	{
		return SourceFile;
	}

	void SetLineNumber(int32 InLineNumber)
	{
		LineNumber = InLineNumber;
	}

	const FUnrealSourceFile& GetUnrealSourceFile() const
	{
		return SourceFile;
	}

private:
	FUnrealSourceFile& SourceFile;
	int32 LineNumber;

	friend struct FUnrealTypeDefinitionInfoArchiveProxy;
};
