// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "FoliageInstancedStaticMeshComponent.generated.h"

class AController;

/**
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(FInstancePointDamageSignature, int32, InstanceIndex, float, Damage, class AController*, InstigatedBy, FVector, HitLocation, FVector, ShotFromDirection, const class UDamageType*, DamageType, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams(FInstanceRadialDamageSignature, const TArray<int32>&, Instances, const TArray<float>&, Damages, class AController*, InstigatedBy, FVector, Origin, float, MaxRadius, const class UDamageType*, DamageType, AActor*, DamageCauser);

UCLASS(ClassGroup = Foliage, Blueprintable)
class FOLIAGE_API UFoliageInstancedStaticMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(BlueprintAssignable, Category = "Game|Damage")
	FInstancePointDamageSignature OnInstanceTakePointDamage;

	UPROPERTY(BlueprintAssignable, Category = "Game|Damage")
	FInstanceRadialDamageSignature OnInstanceTakeRadialDamage;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint64 FoliageHiddenEditorViews;
#endif// WITH_EDITOR

	virtual void ReceiveComponentDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	//USceneComponent Interface
#if WITH_EDITOR
	virtual uint64 GetHiddenEditorViews() const override;
#endif// WITH_EDITOR
};

