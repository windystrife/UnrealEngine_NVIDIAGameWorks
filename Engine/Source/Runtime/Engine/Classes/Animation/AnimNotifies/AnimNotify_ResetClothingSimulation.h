// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNotify.h"

#include "AnimNotify_ResetClothingSimulation.generated.h"

UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Reset Clothing Simulation"))
class ENGINE_API UAnimNotify_ResetClothingSimulation : public UAnimNotify
{
	GENERATED_BODY()

public:

	UAnimNotify_ResetClothingSimulation();

	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

};



