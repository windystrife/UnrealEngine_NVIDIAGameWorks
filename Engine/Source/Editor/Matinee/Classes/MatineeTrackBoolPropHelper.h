// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackBoolPropHelper.generated.h"

class SWindow;
class UInterpGroup;
class UInterpTrack;

UCLASS()
class UMatineeTrackBoolPropHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnCreateTrackTextEntry(const FString& ChosenText, TWeakPtr<SWindow> Window, FString* OutputString);

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const override;
	virtual void  PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const override;
};

