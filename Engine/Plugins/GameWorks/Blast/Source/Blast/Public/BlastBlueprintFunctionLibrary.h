#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlastBlueprintFunctionLibrary.generated.h"

UCLASS()
class BLAST_API UBlastBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//Make sure calling UPrimitiveComponent::AddImpulse would be valid before generating invalid operation warnings
	UFUNCTION(BlueprintPure, Category = "Blast")
	static bool IsValidToApplyForces(class UPrimitiveComponent* Component, FName BoneName);
};

