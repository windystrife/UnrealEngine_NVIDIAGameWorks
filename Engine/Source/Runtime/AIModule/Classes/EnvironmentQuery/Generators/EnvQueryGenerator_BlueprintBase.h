// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/ScriptMacros.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_BlueprintBase.generated.h"

class AActor;

UCLASS(Abstract, Blueprintable)
class AIMODULE_API UEnvQueryGenerator_BlueprintBase : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

	/** A short description of what test does, like "Generate pawn named Joe" */
	UPROPERTY(EditAnywhere, Category = Generator)
	FText GeneratorsActionDescription;

	/** context */
	UPROPERTY(EditAnywhere, Category = Generator)
	TSubclassOf<UEnvQueryContext> Context;

	/** @todo this should show up only in the generator's BP, but 
	 *	due to the way EQS editor is generating widgets it's there as well
	 *	It's a bug and we'll fix it */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	TSubclassOf<UEnvQueryItemType> GeneratedItemType;

	virtual void PostInitProperties() override;
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = Generator)
	void DoItemGeneration(const TArray<FVector>& ContextLocations) const;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
	
	UFUNCTION(BlueprintCallable, Category = "EQS")
	void AddGeneratedVector(FVector GeneratedVector) const;

	UFUNCTION(BlueprintCallable, Category = "EQS")
	void AddGeneratedActor(AActor* GeneratedActor) const;

	UFUNCTION(BlueprintCallable, Category = "EQS")
	UObject* GetQuerier() const;

private:
	/** this is valid and set only within GenerateItems call */
	mutable FEnvQueryInstance* CachedQueryInstance;
};
