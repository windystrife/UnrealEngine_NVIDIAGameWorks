// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleModuleVectorFieldBase: Base class for organizing vector field
		related modules.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/ParticleModule.h"
#include "ParticleModuleVectorFieldBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "Vector Field"))
class UParticleModuleVectorFieldBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

};

