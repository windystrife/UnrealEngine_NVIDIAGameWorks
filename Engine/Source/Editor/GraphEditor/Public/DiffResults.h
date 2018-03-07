// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Differences found within a graph */
namespace EDiffType
{
	/** Differences are presented to the user in the order listed here, so put less important differences lower down */
	enum Type
	{
		NO_DIFFERENCE,
		NODE_REMOVED,
		NODE_ADDED,
		PIN_LINKEDTO_NUM_DEC,
		PIN_LINKEDTO_NUM_INC,
		PIN_DEFAULT_VALUE,
		PIN_TYPE_CATEGORY,
		PIN_TYPE_SUBCATEGORY,
		PIN_TYPE_SUBCATEGORY_OBJECT,
		PIN_TYPE_IS_ARRAY,
		PIN_TYPE_IS_REF,
		PIN_LINKEDTO_NODE,
		NODE_MOVED,
		TIMELINE_LENGTH,
		TIMELINE_AUTOPLAY,
		TIMELINE_LOOP,
		TIMELINE_IGNOREDILATION,
		TIMELINE_NUM_TRACKS,
		TIMELINE_TRACK_MODIFIED,
		NODE_PIN_COUNT,
		NODE_COMMENT,
		NODE_PROPERTY
	};
}

/** Result of a single difference found on graph.*/
struct FDiffSingleResult
{
	FDiffSingleResult()
	{
		Diff = EDiffType::NO_DIFFERENCE;
		Node1 = NULL;
		Node2 = NULL;
		Pin1 = NULL;
		Pin2 = NULL;
		DisplayColor = FLinearColor::White;
	}

	/** The type of diff */
	EDiffType::Type Diff;

	/** The first node involved in diff */
	class UEdGraphNode* Node1;

	/** The second node involved in diff */
	class UEdGraphNode* Node2;

	/** The first pin involved in diff */
	class UEdGraphPin* Pin1;

	/** The second pin involved in diff */
	class UEdGraphPin* Pin2;

	/** String describing the error to the user */
	FText DisplayString;

	/** Optional tooltip containing more information */
	FText ToolTip; 

	/** User can override color to use for display string */
	FLinearColor DisplayColor;

	/** Name of the graph this difference was created on*/
	FName OwningGraph;
};

FORCEINLINE bool operator==( const FDiffSingleResult& LHS, const FDiffSingleResult& RHS )
{
	return	LHS.Diff == RHS.Diff &&
			LHS.Node1 == RHS.Node1 &&
			LHS.Node2 == RHS.Node2 &&
			LHS.Pin1 == RHS.Pin1 &&
			LHS.Pin2 == RHS.Pin2 &&
			LHS.DisplayString.ToString() == RHS.DisplayString.ToString() &&
			LHS.ToolTip.ToString() == RHS.ToolTip.ToString() &&
			LHS.DisplayColor == RHS.DisplayColor;
}

/** Collects the Diffs found for a node */
struct FDiffResults
{
	FDiffResults(TArray<FDiffSingleResult>* InResultArray): ResultArray(InResultArray), bHasFoundDiffs(false)
	{}

	/** Add a diff that was found */
	void Add(const FDiffSingleResult& Result)
	{
		if(Result.Diff != EDiffType::NO_DIFFERENCE)
		{
			bHasFoundDiffs = true;
			if(ResultArray)
			{
				ResultArray->Add(Result);
			}
		}
	}

	/** Test if it can store results*/
	bool CanStoreResults() const
	{
		return ResultArray != nullptr;
	}

	/** Get the number of diffs found*/
	int32 Num() const { return ResultArray ? ResultArray->Num() : 0;}

	/** True if diffs were found */
	bool HasFoundDiffs() const { return bHasFoundDiffs; }

private:
	/** Pointer to optional array, passed in by user to store results in */
	TArray<FDiffSingleResult>* ResultArray;
	bool bHasFoundDiffs;
};
