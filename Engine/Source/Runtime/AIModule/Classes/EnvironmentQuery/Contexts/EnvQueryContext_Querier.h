// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_Querier.generated.h"

struct FEnvQueryContextData;
struct FEnvQueryInstance;

UCLASS(MinimalAPI)
class UEnvQueryContext_Querier : public UEnvQueryContext
{
	GENERATED_UCLASS_BODY()
 
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
