// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify_ResetClothingSimulation.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_ResetClothingSimulation::UAnimNotify_ResetClothingSimulation()
	: Super()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(90, 220, 255, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_ResetClothingSimulation::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	MeshComp->ForceClothNextUpdateTeleportAndReset();
}

FString UAnimNotify_ResetClothingSimulation::GetNotifyName_Implementation() const
{
	return TEXT("Reset Clothing Sim");
}