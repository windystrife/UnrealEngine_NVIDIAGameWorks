// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"

/////////////////////////////////////////////////////
// UAnimNotifyState

UAnimNotifyState::UAnimNotifyState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(200, 200, 255, 255);
#endif // WITH_EDITORONLY_DATA

	bIsNativeBranchingPoint = false;
}


void UAnimNotifyState::NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration)
{
	Received_NotifyBegin(MeshComp, Animation, TotalDuration);
}

void UAnimNotifyState::NotifyTick(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float FrameDeltaTime)
{
	Received_NotifyTick(MeshComp, Animation, FrameDeltaTime);
}

void UAnimNotifyState::NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation)
{
	Received_NotifyEnd(MeshComp, Animation);
}

void UAnimNotifyState::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	NotifyBegin(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, BranchingPointPayload.NotifyEvent ? BranchingPointPayload.NotifyEvent->GetDuration() : 0.f);
}

void UAnimNotifyState::BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime)
{
	NotifyTick(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, FrameDeltaTime);
}

void UAnimNotifyState::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	NotifyEnd(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset);
}

/// @cond DOXYGEN_WARNINGS

FString UAnimNotifyState::GetNotifyName_Implementation() const
{
	UObject* ClassGeneratedBy = GetClass()->ClassGeneratedBy;
	FString NotifyName;

	if(ClassGeneratedBy)
	{
		// GeneratedBy will be valid for blueprint types and gives a clean name without a suffix
		NotifyName = ClassGeneratedBy->GetName();
	}
	else
	{
		// Native notify classes are clean without a suffix otherwise
		NotifyName = GetClass()->GetName();
	}

	NotifyName = NotifyName.Replace(TEXT("AnimNotifyState_"), TEXT(""), ESearchCase::CaseSensitive);
	
	return NotifyName;
}

/// @endcond

void UAnimNotifyState::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	// Ensure that all loaded notifies are transactional
	SetFlags(GetFlags() | RF_Transactional);
#endif
}


