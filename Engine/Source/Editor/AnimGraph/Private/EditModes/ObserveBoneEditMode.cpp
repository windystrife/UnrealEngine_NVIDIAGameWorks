// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditModes/ObserveBoneEditMode.h"
#include "AnimGraphNode_ObserveBone.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"

void FObserveBoneEditMode::EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode)
{
	RuntimeNode = static_cast<FAnimNode_ObserveBone*>(InRuntimeNode);
	GraphNode = CastChecked<UAnimGraphNode_ObserveBone>(InEditorNode);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FObserveBoneEditMode::ExitMode()
{
	RuntimeNode = nullptr;
	GraphNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

ECoordSystem FObserveBoneEditMode::GetWidgetCoordinateSystem() const
{
	switch (GraphNode->Node.DisplaySpace)
	{
	default:
	case BCS_ParentBoneSpace:
		//@TODO: No good way of handling this one
		return COORD_World;
	case BCS_BoneSpace:
		return COORD_Local;
	case BCS_ComponentSpace:
	case BCS_WorldSpace:
		return COORD_World;
	}
}

FVector FObserveBoneEditMode::GetWidgetLocation() const
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	USkeleton* Skeleton = SkelComp->SkeletalMesh->Skeleton;
	FVector WidgetLoc = FVector::ZeroVector;

	int32 MeshBoneIndex = SkelComp->GetBoneIndex(GraphNode->Node.BoneToObserve.BoneName);

	if (MeshBoneIndex != INDEX_NONE)
	{
		const FTransform BoneTM = SkelComp->GetBoneTransform(MeshBoneIndex);
		WidgetLoc = BoneTM.GetLocation();
	}

	return WidgetLoc;
}

FWidget::EWidgetMode FObserveBoneEditMode::GetWidgetMode() const
{
	return FWidget::WM_Translate;
}

FName FObserveBoneEditMode::GetSelectedBone() const
{
	return GraphNode->Node.BoneToObserve.BoneName;
}
