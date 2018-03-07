// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendBoneByChannel.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendBoneByChannel

#define LOCTEXT_NAMESPACE "AnimGraphNode_BlendBoneByChannel"

UAnimGraphNode_BlendBoneByChannel::UAnimGraphNode_BlendBoneByChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAnimGraphNode_BlendBoneByChannel::GetNodeCategory() const
{
	return TEXT("Blends");
}

FLinearColor UAnimGraphNode_BlendBoneByChannel::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.75f);
}

FText UAnimGraphNode_BlendBoneByChannel::GetTooltipText() const
{
	return LOCTEXT("BlendBoneByChannelTooltip", "Blend bones by channel from two poses together.");
}

FText UAnimGraphNode_BlendBoneByChannel::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("BlendBoneByChannel", "Blend Bone By Channel");
}

#undef LOCTEXT_NAMESPACE

