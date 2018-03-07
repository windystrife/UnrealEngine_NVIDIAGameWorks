// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_TwoWayBlend.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_TwoWayBlend

#define LOCTEXT_NAMESPACE "AnimGraphNode_TwoWayBlend"

UAnimGraphNode_TwoWayBlend::UAnimGraphNode_TwoWayBlend(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAnimGraphNode_TwoWayBlend::GetNodeCategory() const
{
	return TEXT("Blends");
}

FLinearColor UAnimGraphNode_TwoWayBlend::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.75f);
}

FText UAnimGraphNode_TwoWayBlend::GetTooltipText() const
{
	return LOCTEXT("TwoWayBlendTooltip", "Blend two poses together");
}

FText UAnimGraphNode_TwoWayBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Blend", "Blend");
}

#undef LOCTEXT_NAMESPACE

