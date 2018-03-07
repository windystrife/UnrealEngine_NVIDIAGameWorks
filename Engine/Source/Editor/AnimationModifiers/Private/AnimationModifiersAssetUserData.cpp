// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationModifiersAssetUserData.h"

void UAnimationModifiersAssetUserData::AddAnimationModifier(UAnimationModifier* Instance)
{
	AnimationModifierInstances.Add(Instance);
}

void UAnimationModifiersAssetUserData::RemoveAnimationModifierInstance(UAnimationModifier* Instance)
{
	checkf(AnimationModifierInstances.Contains(Instance), TEXT("Instance suppose to be removed is not found"));
	AnimationModifierInstances.Remove(Instance);
}

const TArray<UAnimationModifier*>& UAnimationModifiersAssetUserData::GetAnimationModifierInstances() const
{
	return AnimationModifierInstances;
}

void UAnimationModifiersAssetUserData::ChangeAnimationModifierIndex(UAnimationModifier* Instance, int32 Direction)
{
	checkf(AnimationModifierInstances.Contains(Instance), TEXT("Instance suppose to be moved is not found"));
	const int32 CurrentIndex = AnimationModifierInstances.IndexOfByKey(Instance);
	const int32 NewIndex = FMath::Clamp(CurrentIndex + Direction, 0, AnimationModifierInstances.Num() - 1);
	if (CurrentIndex != NewIndex)
	{
		AnimationModifierInstances.Swap(CurrentIndex, NewIndex);
	}
}

void UAnimationModifiersAssetUserData::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	RemoveInvalidModifiers();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimationModifiersAssetUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UAnimationModifiersAssetUserData::PostLoad()
{
	Super::PostLoad();
	RemoveInvalidModifiers();
}

void UAnimationModifiersAssetUserData::RemoveInvalidModifiers()
{
	// This will catch force-deleted blueprints to be removed from our stored array
	AnimationModifierInstances.RemoveAll([](UAnimationModifier* Modifier) { return Modifier == nullptr; });
}
