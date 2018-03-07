// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Particles/ParticleModule.h"
#include "ParticleModuleSubUVBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "SubUV"))
class UParticleModuleSubUVBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

	// Begin UParticleModule Interface
	virtual EModuleType	GetModuleType() const override { return EPMT_SubUV; }
	//End UParticleModule Interface

};

