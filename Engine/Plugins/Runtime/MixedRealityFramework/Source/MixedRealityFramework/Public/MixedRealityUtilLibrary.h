// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MixedRealityUtilLibrary.generated.h"

class UCameraComponent;
class APawn;
class APlayerController;
class UMaterialBillboardComponent;

UCLASS()
class MIXEDREALITYFRAMEWORK_API UMixedRealityUtilLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="MixedReality|Utils")
	static void SetMaterialBillboardSize(UMaterialBillboardComponent* Target, float NewSizeX, float NewSizeY);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Utils")
	static void SanitizeVectorDataSet(UPARAM(ref) TArray<FVector>& VectorArray, const float DivergenceThreshold = 0.01f, const int32 MinSamplCount = 10, const int32 MaxSampleCount = 25, const bool bRecursive = true);

public:
	static APawn* FindAssociatedPlayerPawn(AActor* ActorInst);
	static USceneComponent* FindAssociatedHMDRoot(AActor* ActorInst);
	static USceneComponent* GetHMDRootComponent(const UObject* WorldContextObject, int32 PlayerIndex);
	static USceneComponent* GetHMDRootComponentFromPlayer(const APlayerController* Player);
	static UCameraComponent* GetHMDCameraComponent(const APawn* PlayerPawn);
	static FTransform GetVRDeviceToWorldTransform(const UObject* WorldContextObject, int32 PlayerIndex);
	static FTransform GetVRDeviceToWorldTransformFromPlayer(const APlayerController* Player);
};
