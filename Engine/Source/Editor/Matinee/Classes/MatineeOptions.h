// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Options for Matinee editor
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Matinee/InterpGroup.h"
#include "MatineeOptions.generated.h"

UCLASS(hidecategories=Object, config=Editor)
class UMatineeOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<struct FInterpEdSelKey> SelectedKeys;

	// Are we currently editing the value of a keyframe. This should only be true if there is one keyframe selected and the time is currently set to it.
	UPROPERTY()
	uint32 bAdjustingKeyframe:1;

	// Are we currently editing the values of a groups keyframe. This should only be true if the keyframes that are selected belong to the same group.
	UPROPERTY()
	uint32 bAdjustingGroupKeyframes:1;

};

