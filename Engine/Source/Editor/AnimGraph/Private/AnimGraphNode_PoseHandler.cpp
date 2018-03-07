// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseHandler.h"
#include "Animation/PoseAsset.h"
#include "AnimNodes/AnimNode_PoseHandler.h"
#include "Kismet2/CompilerResultsLog.h"

UAnimGraphNode_PoseHandler::UAnimGraphNode_PoseHandler(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

void UAnimGraphNode_PoseHandler::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	UPoseAsset* PoseAssetToCheck = GetPoseHandlerNode()->PoseAsset;
	UEdGraphPin* PoseAssetPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseHandler, PoseAsset));
	if (PoseAssetPin != nullptr && PoseAssetToCheck == nullptr)
	{
		PoseAssetToCheck = Cast<UPoseAsset>(PoseAssetPin->DefaultObject);
	}

	if (PoseAssetToCheck == nullptr)
	{
		if (IsPoseAssetRequired() && (PoseAssetPin == nullptr || PoseAssetPin->LinkedTo.Num() == 0))
		{
			MessageLog.Error(TEXT("@@ references an unknown poseasset"), this);
		}
	}
	else
	{
		USkeleton* SeqSkeleton = PoseAssetToCheck->GetSkeleton();
		if (SeqSkeleton && // if PoseAsset doesn't have skeleton, it might be due to PoseAsset not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!SeqSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references poseasset that uses different skeleton @@"), this, SeqSkeleton);
		}
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_PoseHandler::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(Asset))
	{
		GetPoseHandlerNode()->PoseAsset = PoseAsset;
	}
}

void UAnimGraphNode_PoseHandler::PreloadRequiredAssets()
{
	PreloadObject(GetPoseHandlerNode()->PoseAsset);

	Super::PreloadRequiredAssets();
}

UAnimationAsset* UAnimGraphNode_PoseHandler::GetAnimationAsset() const
{
	UPoseAsset* PoseAsset = GetPoseHandlerNode()->PoseAsset;
	UEdGraphPin* PoseAssetPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseHandler, PoseAsset));
	if (PoseAssetPin != nullptr && PoseAsset == nullptr)
	{
		PoseAsset = Cast<UPoseAsset>(PoseAssetPin->DefaultObject);
	}

	return PoseAsset;
}
