// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimationGraphSchema.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_LocalToComponentSpace

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_LocalToComponentSpace::UAnimGraphNode_LocalToComponentSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_LocalToComponentSpace::GetNodeTitleColor() const
{
	return FLinearColor(0.7f, 0.7f, 0.7f);
}

FText UAnimGraphNode_LocalToComponentSpace::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_LocalToComponentSpace_Tooltip", "Convert Local Pose to Component Space Pose");
}

FText UAnimGraphNode_LocalToComponentSpace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_LocalToComponentSpace_Title", "Local To Component");
}

FString UAnimGraphNode_LocalToComponentSpace::GetNodeCategory() const
{
	return TEXT("Convert Spaces");
}

void UAnimGraphNode_LocalToComponentSpace::CreateOutputPins()
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	CreatePin(EGPD_Output, Schema->PC_Struct, FString(), FComponentSpacePoseLink::StaticStruct(), TEXT("ComponentPose"));
}

void UAnimGraphNode_LocalToComponentSpace::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	DisplayName.Reset();
}

#undef LOCTEXT_NAMESPACE
