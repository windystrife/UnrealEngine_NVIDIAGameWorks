// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackParticleReplayHelper.generated.h"

class UInterpTrack;

UCLASS()
class UMatineeTrackParticleReplayHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const override;
	virtual void  PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const override;
};
