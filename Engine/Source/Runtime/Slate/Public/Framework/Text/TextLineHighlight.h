// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextRange.h"

class ILineHighlighter;

struct SLATE_API FTextLineHighlight
{
	FTextLineHighlight( int32 InLineIndex, const FTextRange& InRange, int32 InZOrder, const TSharedRef< ILineHighlighter >& InHighlighter )
		: LineIndex( InLineIndex )
		, Range( InRange )
		, ZOrder( InZOrder )
		, Highlighter( InHighlighter )
	{

	}

	bool operator==(const FTextLineHighlight& Other) const
	{
		return LineIndex == Other.LineIndex
			&& Range == Other.Range
			&& ZOrder == Other.ZOrder
			&& Highlighter == Other.Highlighter;
	}

	bool operator!=(const FTextLineHighlight& Other) const
	{
		return LineIndex != Other.LineIndex
			|| Range != Other.Range
			|| ZOrder != Other.ZOrder
			|| Highlighter != Other.Highlighter;
	}

	int32 LineIndex;
	FTextRange Range;
	int32 ZOrder;
	TSharedRef< ILineHighlighter > Highlighter;
};
