// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWindow.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackFloatPropHelper.generated.h"

class UInterpGroup;
class UInterpTrack;

UCLASS()
class UMatineeTrackFloatPropHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnCreateTrackTextEntry(const FString& ChosenText, TSharedRef<SWindow> Window, FString* OutputString);

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const override;
	virtual void  PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const override;
};

