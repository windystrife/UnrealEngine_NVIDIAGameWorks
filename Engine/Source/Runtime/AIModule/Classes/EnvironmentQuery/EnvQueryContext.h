// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EnvQueryContext.generated.h"

struct FEnvQueryContextData;
struct FEnvQueryInstance;

UCLASS(Abstract, EditInlineNew)
class AIMODULE_API UEnvQueryContext : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const;
};
