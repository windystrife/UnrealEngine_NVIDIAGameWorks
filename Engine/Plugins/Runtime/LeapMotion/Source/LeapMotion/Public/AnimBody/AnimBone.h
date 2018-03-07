// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "AnimBone.generated.h"

UCLASS(BlueprintType)
class UAnimBone : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
	FVector Position;

	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
	FRotator Orientation;

	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
	FVector Scale;

	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
	float Length;

	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
	float Alpha;


	//Optional vector of the next joint (outward)
	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
		FVector NextJoint;

	//Optional vector of the previous joint (inward)
	UPROPERTY(BlueprintReadWrite, Category = "Anim Bone")
		FVector PrevJoint;

	UFUNCTION(BlueprintCallable, Category = "Anim Bone")
	bool Enabled();

	UFUNCTION(BlueprintCallable, Category = "Anim Bone")
	void SetEnabled(bool Enable = true);

	UFUNCTION(BlueprintCallable, Category = "Anim Bone")
	void SetFromTransform(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category = "Anim Bone")
	FTransform GetTransform();

	UFUNCTION(BlueprintCallable, Category = "Anim Bone")
	void TranslateBone(FVector Shift);

	UFUNCTION(BlueprintCallable, Category = "Anim Bone")
	void ChangeBasis(FRotator PreBase, FRotator PostBase, bool AdjustVectors = true);
};