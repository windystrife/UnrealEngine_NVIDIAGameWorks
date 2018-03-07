// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AnimHand.h"
#include "AnimBody.generated.h"

class UAnimBone;

//NB: this is a limited class used only for leap anim, full class will have full body

UCLASS(ClassGroup = "Animation Skeleton", meta = (BlueprintSpawnableComponent))
class UAnimBody : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Anim Body")
	float Alpha;

	//Hands
	UPROPERTY(BlueprintReadOnly, Category = "Anim Body")
	UAnimHand* Left;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Body")
	UAnimHand* Right;

	//Head
	UPROPERTY(BlueprintReadOnly, Category = "Anim Body")
	UAnimBone* Head;


	UFUNCTION(BlueprintCallable, Category = "Anim Body")
	bool Enabled();

	UFUNCTION(BlueprintCallable, Category = "Anim Body")
	void SetEnabled(bool Enable = true);

	UFUNCTION(BlueprintCallable, Category = "Anim Body")
	void TranslateBody(FVector Shift);

	UFUNCTION(BlueprintCallable, Category = "Anim Body")
	void ChangeBasis(FRotator PreBase, FRotator PostBase, bool AdjustVectors = true);
};