// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Distributions/DistributionVectorParameterBase.h"
#include "DistributionVectorParticleParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, editinlinenew)
class ENGINE_API UDistributionVectorParticleParameter : public UDistributionVectorParameterBase
{
	GENERATED_UCLASS_BODY()


	//~ Begin UDistributionVectorParameterBase Interface
	virtual bool GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) const override;
	//~ End UDistributionVectorParameterBase Interface
};

