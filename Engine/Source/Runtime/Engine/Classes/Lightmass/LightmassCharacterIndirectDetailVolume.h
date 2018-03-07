// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LightmassCharacterIndirectDetailVolume:  Defines areas where Lightmass should place more indirect lighting samples than normal.
//=============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"
#include "LightmassCharacterIndirectDetailVolume.generated.h"

UCLASS(hidecategories=(Collision, Brush, Attachment, Physics, Volume), MinimalAPI)
class ALightmassCharacterIndirectDetailVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

};



