// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_ApplyAdditive.h"

#include "Animation/AnimationSettings.h"
#include "Kismet2/CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_ApplyAdditive

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_ApplyAdditive::UAnimGraphNode_ApplyAdditive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_ApplyAdditive::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.75f);
}

FText UAnimGraphNode_ApplyAdditive::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_ApplyAdditive_Tooltip", "Apply additive animation to normal pose");
}

FText UAnimGraphNode_ApplyAdditive::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_ApplyAdditive_Title", "Apply Additive");
}

FString UAnimGraphNode_ApplyAdditive::GetNodeCategory() const
{
	return TEXT("Blends");
	//@TODO: TEXT("Apply additive to normal pose"), TEXT("Apply additive pose"));
}

void UAnimGraphNode_ApplyAdditive::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (UAnimationSettings::Get()->bEnablePerformanceLog)
	{
		if (Node.LODThreshold < 0)
		{
			MessageLog.Warning(TEXT("@@ contains no LOD Threshold."), this);
		}
	}
}
#undef LOCTEXT_NAMESPACE
