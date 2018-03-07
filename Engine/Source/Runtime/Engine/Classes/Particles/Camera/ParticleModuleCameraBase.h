// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/ParticleModule.h"
#include "ParticleModuleCameraBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "Camera"))
class UParticleModuleCameraBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

	virtual bool CanTickInAnyThread() override
	{
		return false;
	}
};

