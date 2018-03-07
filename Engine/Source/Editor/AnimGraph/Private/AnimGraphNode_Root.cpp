// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_Root.h"
#include "GraphEditorSettings.h"


/////////////////////////////////////////////////////
// FPoseLinkMappingRecord

void FPoseLinkMappingRecord::PatchLinkIndex(uint8* DestinationPtr, int32 LinkID, int32 SourceLinkID) const
{
	checkSlow(IsValid());

	DestinationPtr = ChildProperty->ContainerPtrToValuePtr<uint8>(DestinationPtr);
	
	if (ChildPropertyIndex != INDEX_NONE)
	{
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(ChildProperty);

		FScriptArrayHelper ArrayHelper(ArrayProperty, DestinationPtr);
		check(ArrayHelper.IsValidIndex(ChildPropertyIndex));

		DestinationPtr = ArrayHelper.GetRawPtr(ChildPropertyIndex);
	}

	// Check to guard against accidental infinite loops
	check((LinkID == INDEX_NONE) || (LinkID != SourceLinkID));

	// Patch the pose link
	FPoseLinkBase& PoseLink = *((FPoseLinkBase*)DestinationPtr);
	PoseLink.LinkID = LinkID;
	PoseLink.SourceLinkID = SourceLinkID;
}

/////////////////////////////////////////////////////
// UAnimGraphNode_Root

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_Root::UAnimGraphNode_Root(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_Root::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UAnimGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNodeRoot_Title", "Final Animation Pose");
}

FText UAnimGraphNode_Root::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNodeRoot_Tooltip", "Wire the final animation pose into this node");
}

bool UAnimGraphNode_Root::IsSinkNode() const
{
	return true;
}

void UAnimGraphNode_Root::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Intentionally empty. This node is auto-generated when a new graph is created.
}

FString UAnimGraphNode_Root::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/AnimationStateMachine");
}

#undef LOCTEXT_NAMESPACE
