// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_MakeDynamicAdditive.h"

#include "Animation/AnimationSettings.h"
#include "Kismet2/CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_MakeDynamicAdditive

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_MakeDynamicAdditive::UAnimGraphNode_MakeDynamicAdditive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_MakeDynamicAdditive::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.75f);
}

FText UAnimGraphNode_MakeDynamicAdditive::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_MakeDynamicAdditive_Tooltip", "Create a dynamic additive pose (additive pose - base pose)");
}

FText UAnimGraphNode_MakeDynamicAdditive::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_MakeDynamicAdditive_Title", "Make Dynamic Additive");
}

FString UAnimGraphNode_MakeDynamicAdditive::GetNodeCategory() const
{
	return TEXT("Blends");
}

void UAnimGraphNode_MakeDynamicAdditive::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
}
#undef LOCTEXT_NAMESPACE
