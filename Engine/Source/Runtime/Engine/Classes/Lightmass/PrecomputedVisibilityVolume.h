// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// PrecomputedVisibilityVolume:  Indicates where visibility should be precomputed
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"
#include "PrecomputedVisibilityVolume.generated.h"

UCLASS(hidecategories=(Collision, Brush, Attachment, Physics, Volume), MinimalAPI)
class APrecomputedVisibilityVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

};



