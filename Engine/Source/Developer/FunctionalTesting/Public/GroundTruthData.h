// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "GroundTruthData.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class FUNCTIONALTESTING_API UGroundTruthData : public UObject
{
	GENERATED_BODY()

public:
	UGroundTruthData();

	UFUNCTION(BlueprintCallable, Category = "Automation")
	void SaveObject(UObject* GroundTruth);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	UObject* LoadObject();

	UFUNCTION(BlueprintCallable, Category = "Automation")
	bool CanModify() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	UPROPERTY(EditAnywhere, Category = Data)
	bool bResetGroundTruth;

protected:
	
	UPROPERTY(VisibleAnywhere, Instanced, Category=Data)
	UObject* ObjectData;
};
