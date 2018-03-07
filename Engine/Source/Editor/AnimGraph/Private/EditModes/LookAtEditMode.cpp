// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditModes/LookAtEditMode.h"
#include "AnimGraphNode_LookAt.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"

void FLookAtEditMode::EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode)
{
	RuntimeNode = static_cast<FAnimNode_LookAt*>(InRuntimeNode);
	GraphNode = CastChecked<UAnimGraphNode_LookAt>(InEditorNode);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FLookAtEditMode::ExitMode()
{
	RuntimeNode = nullptr;
	GraphNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

FVector FLookAtEditMode::GetWidgetLocation() const
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	return GraphNode->Node.GetCachedTargetLocation();
}

FWidget::EWidgetMode FLookAtEditMode::GetWidgetMode() const
{
	return FWidget::WM_Translate;
}

FName FLookAtEditMode::GetSelectedBone() const
{
	return GraphNode->Node.BoneToModify.BoneName;
}

// @todo: will support this since now we have LookAtLocation
void FLookAtEditMode::DoTranslation(FVector& InTranslation)
{
}
