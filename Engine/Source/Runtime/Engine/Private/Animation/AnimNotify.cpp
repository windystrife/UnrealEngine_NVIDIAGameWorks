// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"

/////////////////////////////////////////////////////
// UAnimNotify

UAnimNotify::UAnimNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MeshContext(NULL)
{

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 200, 200, 255);
#endif // WITH_EDITORONLY_DATA

	bIsNativeBranchingPoint = false;
}

void UAnimNotify::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	USkeletalMeshComponent* PrevContext = MeshContext;
	MeshContext = MeshComp;
	Received_Notify(MeshComp, Animation);
	MeshContext = PrevContext;
}

void UAnimNotify::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Notify(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset);
}

class UWorld* UAnimNotify::GetWorld() const
{
	return (MeshContext ? MeshContext->GetWorld() : NULL);
}

/// @cond DOXYGEN_WARNINGS

FString UAnimNotify::GetNotifyName_Implementation() const
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

	NotifyName = NotifyName.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
	
	return NotifyName;
}

/// @endcond

void UAnimNotify::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	// Ensure that all loaded notifies are transactional
	SetFlags(GetFlags() | RF_Transactional);
#endif
}
