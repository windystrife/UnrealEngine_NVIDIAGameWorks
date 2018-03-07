// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRemapAsset.h"
#include "LiveLinkTypes.h"
#include "BonePose.h"

ULiveLinkRemapAsset::ULiveLinkRemapAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
	if (Blueprint)
	{
		OnBlueprintCompiledDelegate = Blueprint->OnCompiled().AddUObject(this, &ULiveLinkRemapAsset::OnBlueprintClassCompiled);
	}
}

void ULiveLinkRemapAsset::BeginDestroy()
{
	if (OnBlueprintCompiledDelegate.IsValid())
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		check(Blueprint);
		Blueprint->OnCompiled().Remove(OnBlueprintCompiledDelegate);
		OnBlueprintCompiledDelegate.Reset();
	}

	Super::BeginDestroy();
}

void ULiveLinkRemapAsset::OnBlueprintClassCompiled(UBlueprint* TargetBlueprint)
{
	NameMap.Reset();
}

void ULiveLinkRemapAsset::BuildPoseForSubject(const FLiveLinkSubjectFrame& InFrame, FCompactPose& OutPose, FBlendedCurve& OutCurve)
{
	const TArray<FName>& SourceBoneNames = InFrame.RefSkeleton.GetBoneNames();

	if ((SourceBoneNames.Num() == 0) || (InFrame.Transforms.Num() == 0) || (SourceBoneNames.Num() != InFrame.Transforms.Num()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to get live link data %i %i"), SourceBoneNames.Num(), InFrame.Transforms.Num());
		return;
	}

	TArray<FName, TMemStackAllocator<>> TransformedBoneNames;
	TransformedBoneNames.Reserve(SourceBoneNames.Num());

	for (const FName& SrcBoneName : SourceBoneNames)
	{
		FName* TargetBoneName = NameMap.Find(SrcBoneName);
		if (TargetBoneName == nullptr)
		{
			FName NewName = GetRemappedBoneName(SrcBoneName);
			TransformedBoneNames.Add(NewName);
			NameMap.Add(SrcBoneName, NewName);
		}
		else
		{
			TransformedBoneNames.Add(*TargetBoneName);
		}
	}

	for (int32 i = 0; i < TransformedBoneNames.Num(); ++i)
	{
		FName BoneName = TransformedBoneNames[i];

		FTransform BoneTransform = InFrame.Transforms[i];

		int32 MeshIndex = OutPose.GetBoneContainer().GetPoseBoneIndexForBoneName(BoneName);
		if (MeshIndex != INDEX_NONE)
		{
			FCompactPoseBoneIndex CPIndex = OutPose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (CPIndex != INDEX_NONE)
			{
				OutPose[CPIndex] = BoneTransform;
			}
		}
	}

	BuildCurveData(InFrame, OutPose, OutCurve);
}

FName ULiveLinkRemapAsset::GetRemappedBoneName_Implementation(FName BoneName) const
{
	return BoneName;
}