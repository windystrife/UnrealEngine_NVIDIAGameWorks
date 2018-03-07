// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Types/SlateEnums.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackEventHelper.generated.h"

class IMatineeBase;
class UInterpTrack;

UCLASS()
class UMatineeTrackEventHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnAddKeyTextEntry(const FText& ChosenText, ETextCommit::Type CommitInfo, IMatineeBase* Matinee, UInterpTrack* Track);

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const override;
	virtual void  PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const override;
};
