// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LightmassImportanceVolume:  a bounding volume outside of which Lightmass
// photon emissions are decreased
//=============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"
#include "LightmassImportanceVolume.generated.h"

UCLASS(hidecategories=(Collision, Brush, Attachment, Physics, Volume), MinimalAPI)
class ALightmassImportanceVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

};



