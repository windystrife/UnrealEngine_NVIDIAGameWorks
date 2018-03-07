// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSphericalParticleOpacity.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionSphericalParticleOpacity : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Density of the particle sphere. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstantDensity' if not specified"))
	FExpressionInput Density;

	/** Constant density of the particle sphere.  Will be overridden if Density is connected. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionSphericalParticleOpacity, meta=(OverridingInputProperty = "Density"))
	float ConstantDensity;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override
	{
		OutCaptions.Add(TEXT("Spherical Particle Opacity"));
	}
#endif
	//~ End UMaterialExpression Interface
};



