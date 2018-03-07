// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Distributions/DistributionVector.h"
#include "Particles/Kill/ParticleModuleKillBase.h"
#include "ParticleModuleKillBox.generated.h"

struct FParticleEmitterInstance;

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Kill Box"))
class UParticleModuleKillBox : public UParticleModuleKillBase
{
	GENERATED_UCLASS_BODY()

	/** The lower left corner of the box. */
	UPROPERTY(EditAnywhere, Category=Kill)
	struct FRawDistributionVector LowerLeftCorner;

	/** The upper right corner of the box. */
	UPROPERTY(EditAnywhere, Category=Kill)
	struct FRawDistributionVector UpperRightCorner;

	/** If true, the box coordinates are in world space./ */
	UPROPERTY(EditAnywhere, Category=Kill)
	uint32 bAbsolute:1;

	/**
	 *	If true, particles INSIDE the box will be killed. 
	 *	If false (the default), particles OUTSIDE the box will be killed.
	 */
	UPROPERTY(EditAnywhere, Category=Kill)
	uint32 bKillInside:1;

	/** If true, the box will always be axis aligned and non-scalable. */
	UPROPERTY(EditAnywhere, Category=Kill)
	uint32 bAxisAlignedAndFixedSize:1;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInitProperties() override;
	//End UObject Interface

	//Begin UParticleModule Interface
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual void	Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	//End UParticleModule Interface
};



