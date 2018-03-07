// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackDirectorHelper.generated.h"

class IMatineeBase;
class UInterpTrack;

UCLASS()
class UMatineeTrackDirectorHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnAddKeyTextEntry(const FString& ChosenText, IMatineeBase* Matinee, UInterpTrack* Track);

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const override;
	virtual void  PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const override;
};
