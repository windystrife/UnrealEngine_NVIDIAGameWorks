// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimClassInterface.h"
#include "AnimClassData.generated.h"

class USkeleton;

UCLASS()
class ENGINE_API UAnimClassData : public UObject, public IAnimClassInterface
{
	GENERATED_BODY()
public:
	// List of state machines present in this blueprint class
	UPROPERTY()
	TArray<FBakedAnimationStateMachine> BakedStateMachines;

	/** Target skeleton for this blueprint class */
	UPROPERTY()
	class USkeleton* TargetSkeleton;

	/** A list of anim notifies that state machines (or anything else) may reference */
	UPROPERTY()
	TArray<FAnimNotifyEvent> AnimNotifies;

	// The index of the root node in the animation tree
	UPROPERTY()
	int32 RootAnimNodeIndex;
	
	// Indices for each of the saved pose nodes that require updating, in the order they need to get updates.
	UPROPERTY()
	TArray<int32> OrderedSavedPoseIndices;

	UPROPERTY()
	UStructProperty* RootAnimNodeProperty;

	// The array of anim nodes
	UPROPERTY()
	TArray<UStructProperty*> AnimNodeProperties;

	// Array of sync group names in the order that they are requested during compile
	UPROPERTY()
	TArray<FName> SyncGroupNames;

public:

	virtual const TArray<FBakedAnimationStateMachine>& GetBakedStateMachines() const override { return BakedStateMachines; }

	virtual USkeleton* GetTargetSkeleton() const override { return TargetSkeleton; }

	virtual const TArray<FAnimNotifyEvent>& GetAnimNotifies() const override { return AnimNotifies; }

	virtual int32 GetRootAnimNodeIndex() const override { return RootAnimNodeIndex; }

	virtual UStructProperty* GetRootAnimNodeProperty() const override { return RootAnimNodeProperty; }

	virtual const TArray<int32>& GetOrderedSavedPoseNodeIndices() const override { return OrderedSavedPoseIndices; }

	virtual const TArray<UStructProperty*>& GetAnimNodeProperties() const override { return AnimNodeProperties; }

	virtual const TArray<FName>& GetSyncGroupNames() const override { return SyncGroupNames; }

	virtual int32 GetSyncGroupIndex(FName SyncGroupName) const override { return SyncGroupNames.IndexOfByKey(SyncGroupName); }

#if WITH_EDITOR
	void CopyFrom(IAnimClassInterface* AnimClass)
	{
		check(AnimClass);
		BakedStateMachines = AnimClass->GetBakedStateMachines();
		TargetSkeleton = AnimClass->GetTargetSkeleton();
		AnimNotifies = AnimClass->GetAnimNotifies();
		RootAnimNodeIndex = AnimClass->GetRootAnimNodeIndex();
		RootAnimNodeProperty = AnimClass->GetRootAnimNodeProperty();
		OrderedSavedPoseIndices = AnimClass->GetOrderedSavedPoseNodeIndices();
		AnimNodeProperties = AnimClass->GetAnimNodeProperties();
		SyncGroupNames = AnimClass->GetSyncGroupNames();
	}
#endif // WITH_EDITOR
};
