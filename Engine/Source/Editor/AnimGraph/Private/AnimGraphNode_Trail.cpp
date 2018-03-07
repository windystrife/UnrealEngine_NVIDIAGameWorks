// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_Trail.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_TrailBone

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_Trail::UAnimGraphNode_Trail(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_Trail::GetControllerDescription() const
{
	return LOCTEXT("TrailController", "Trail controller");
}

FText UAnimGraphNode_Trail::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_Trail_Tooltip", "The Trail Controller.");
}

FText UAnimGraphNode_Trail::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle) && (Node.TrailBone.BoneName == NAME_None))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.TrailBone.BoneName));

		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_Trail_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_Trail_Title", "{ControllerDescription}\nBone: {BoneName}"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_Trail::PostLoad()
{
	Super::PostLoad();
	Node.PostLoad();
}
#undef LOCTEXT_NAMESPACE
