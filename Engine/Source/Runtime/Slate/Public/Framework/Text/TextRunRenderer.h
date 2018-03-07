// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextRange.h"

class IRunRenderer;

struct SLATE_API FTextRunRenderer
{
	FTextRunRenderer( int32 InLineIndex, const FTextRange& InRange, const TSharedRef< IRunRenderer >& InRenderer )
		: LineIndex( InLineIndex )
		, Range( InRange )
		, Renderer( InRenderer )
	{

	}

	bool operator==(const FTextRunRenderer& Other) const
	{
		return LineIndex == Other.LineIndex
			&& Range == Other.Range
			&& Renderer == Other.Renderer;
	}

	bool operator!=(const FTextRunRenderer& Other) const
	{
		return LineIndex != Other.LineIndex
			|| Range != Other.Range
			|| Renderer != Other.Renderer;
	}

	int32 LineIndex;
	FTextRange Range;
	TSharedRef< IRunRenderer > Renderer;
};
