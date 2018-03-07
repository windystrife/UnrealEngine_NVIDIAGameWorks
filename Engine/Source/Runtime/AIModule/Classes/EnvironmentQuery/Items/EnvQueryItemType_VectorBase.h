// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvQueryItemType_VectorBase.generated.h"

class UBlackboardComponent;
struct FBlackboardKeySelector;

UCLASS(Abstract)
class AIMODULE_API UEnvQueryItemType_VectorBase : public UEnvQueryItemType
{
	GENERATED_BODY()

public:
	virtual FVector GetItemLocation(const uint8* RawData) const;
	virtual FRotator GetItemRotation(const uint8* RawData) const;

	virtual void AddBlackboardFilters(FBlackboardKeySelector& KeySelector, UObject* FilterOwner) const override;
	virtual bool StoreInBlackboard(FBlackboardKeySelector& KeySelector, UBlackboardComponent* Blackboard, const uint8* RawData) const override;
	virtual FString GetDescription(const uint8* RawData) const override;
};
