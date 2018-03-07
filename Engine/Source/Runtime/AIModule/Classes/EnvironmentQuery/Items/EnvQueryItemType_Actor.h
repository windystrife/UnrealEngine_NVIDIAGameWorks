// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvQueryItemType_Actor.generated.h"

class AActor;
struct FEnvQueryContextData;
struct FWeakObjectPtr;

UCLASS()
class AIMODULE_API UEnvQueryItemType_Actor : public UEnvQueryItemType_ActorBase
{
	GENERATED_BODY()
public:
	typedef const FWeakObjectPtr& FValueType;

	UEnvQueryItemType_Actor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static AActor* GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FWeakObjectPtr& Value);

	static void SetContextHelper(FEnvQueryContextData& ContextData, const AActor* SingleActor);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<const AActor*>& MultipleActors);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<AActor*>& MultipleActors);

	virtual FVector GetItemLocation(const uint8* RawData) const override;
	virtual FRotator GetItemRotation(const uint8* RawData) const override;
	virtual AActor* GetActor(const uint8* RawData) const override;
};
