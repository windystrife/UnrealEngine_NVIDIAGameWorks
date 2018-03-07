// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/SlateDelegates.h"

template< typename ObjectType > class TAttribute;
class UMovieSceneTrack;
class UMovieSceneSection;
class ISequencer;
class FMenuBuilder;

struct SEQUENCER_API FSequencerUtilities
{
	static TSharedRef<SWidget> MakeAddButton(FText HoverText, FOnGetContent MenuContent, const TAttribute<bool>& HoverState);

	static void PopulateMenu_CreateNewSection(FMenuBuilder& MenuBuilder, int32 RowIndex, UMovieSceneTrack* Track, TWeakPtr<ISequencer> InSequencer);

	static void PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, UMovieSceneSection* Section);

	static void PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UMovieSceneSection>>& InSections);

	/** 
	 * Generates a unique FName from a candidate name given a set of already existing names.  
	 * The name is made unique by appending a number to the end.
	 */
	static FName GetUniqueName(FName CandidateName, const TArray<FName>& ExistingNames);
};
