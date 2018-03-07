// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorSkeletonTreeBuilder.h"
#include "SkeletonTreePhysicsBodyItem.h"
#include "SkeletonTreePhysicsShapeItem.h"
#include "SkeletonTreePhysicsConstraintItem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "TextFilterExpressionEvaluator.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetEditorSkeletonTreeBuilder"

FPhysicsAssetEditorSkeletonTreeBuilder::FPhysicsAssetEditorSkeletonTreeBuilder(UPhysicsAsset* InPhysicsAsset, const FSkeletonTreeBuilderArgs& InSkeletonTreeBuilderArgs)
	: FSkeletonTreeBuilder(InSkeletonTreeBuilderArgs)
	, bShowBodies(true)
	, bShowConstraints(true)
	, bShowPrimitives(true)
	, PhysicsAsset(InPhysicsAsset)
{
}

void FPhysicsAssetEditorSkeletonTreeBuilder::Build(FSkeletonTreeBuilderOutput& Output)
{
	if(BuilderArgs.bShowBones)
	{
		AddBones(Output);
	}

	AddBodies(Output);

	if(BuilderArgs.bShowAttachedAssets)
	{
		AddAttachedAssets(Output);
	}
}

ESkeletonTreeFilterResult FPhysicsAssetEditorSkeletonTreeBuilder::FilterItem(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<class ISkeletonTreeItem>& InItem)
{
	if(InItem->IsOfType<FSkeletonTreePhysicsBodyItem>() || InItem->IsOfType<FSkeletonTreePhysicsConstraintItem>() || InItem->IsOfType<FSkeletonTreePhysicsShapeItem>())
	{
		ESkeletonTreeFilterResult Result = ESkeletonTreeFilterResult::Shown;

		if (InArgs.TextFilter.IsValid())
		{
			if (InArgs.TextFilter->TestTextFilter(FSkeletonTreeFilterContext(InItem->GetRowItemName())))
			{
				Result = ESkeletonTreeFilterResult::ShownHighlighted;
			}
			else
			{
				Result = ESkeletonTreeFilterResult::Hidden;
			}
		}

		if(InItem->IsOfType<FSkeletonTreePhysicsBodyItem>())
		{
			if(!bShowBodies)
			{
				Result = ESkeletonTreeFilterResult::Hidden;
			}
		}
		else if(InItem->IsOfType<FSkeletonTreePhysicsConstraintItem>())
		{
			if(!bShowConstraints)
			{
				Result = ESkeletonTreeFilterResult::Hidden;
			}
		}
		else if(InItem->IsOfType<FSkeletonTreePhysicsShapeItem>())
		{
			if(!bShowPrimitives)
			{
				Result = ESkeletonTreeFilterResult::Hidden;
			}
		}

		return Result;
	}

	return FSkeletonTreeBuilder::FilterItem(InArgs, InItem);
}

void FPhysicsAssetEditorSkeletonTreeBuilder::AddBodies(FSkeletonTreeBuilderOutput& Output)
{
	if (PreviewScenePtr.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		if (PreviewMeshComponent->SkeletalMesh)
		{
			FReferenceSkeleton& RefSkeleton = PreviewMeshComponent->SkeletalMesh->RefSkeleton;

			for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex)
			{
				const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
				int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				const FName ParentName = ParentIndex == INDEX_NONE ? NAME_None : RefSkeleton.GetBoneName(ParentIndex);

				bool bHasBodySetup = false;
				for (int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAsset->SkeletalBodySetups.Num(); ++BodySetupIndex)
				{
					if (BoneName == PhysicsAsset->SkeletalBodySetups[BodySetupIndex]->BoneName)
					{
						USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodySetupIndex];

						bHasBodySetup = true;

						const FKAggregateGeom& AggGeom = PhysicsAsset->SkeletalBodySetups[BodySetupIndex]->AggGeom;
						
						bool bHasShapes = AggGeom.SphereElems.Num() > 0 || AggGeom.BoxElems.Num() > 0 || AggGeom.SphylElems.Num() > 0 || AggGeom.ConvexElems.Num() > 0;

						if (bHasShapes)
						{
							Output.Add(MakeShared<FSkeletonTreePhysicsBodyItem>(BodySetup, BodySetupIndex, BoneName, true, bHasShapes, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, "FSkeletonTreeBoneItem", true);

							int32 ShapeIndex;
							for (ShapeIndex = 0; ShapeIndex < AggGeom.SphereElems.Num(); ++ShapeIndex)
							{
								Output.Add(MakeShared<FSkeletonTreePhysicsShapeItem>(BodySetup, BoneName, BodySetupIndex, EAggCollisionShape::Sphere, ShapeIndex, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, FSkeletonTreePhysicsBodyItem::GetTypeId());
							}

							for (ShapeIndex = 0; ShapeIndex < AggGeom.BoxElems.Num(); ++ShapeIndex)
							{
								Output.Add(MakeShared<FSkeletonTreePhysicsShapeItem>(BodySetup, BoneName, BodySetupIndex, EAggCollisionShape::Box, ShapeIndex, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, FSkeletonTreePhysicsBodyItem::GetTypeId());
							}

							for (ShapeIndex = 0; ShapeIndex < AggGeom.SphylElems.Num(); ++ShapeIndex)
							{
								Output.Add(MakeShared<FSkeletonTreePhysicsShapeItem>(BodySetup, BoneName, BodySetupIndex, EAggCollisionShape::Sphyl, ShapeIndex, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, FSkeletonTreePhysicsBodyItem::GetTypeId());
							}

							for (ShapeIndex = 0; ShapeIndex < AggGeom.ConvexElems.Num(); ++ShapeIndex)
							{
								Output.Add(MakeShared<FSkeletonTreePhysicsShapeItem>(BodySetup, BoneName, BodySetupIndex, EAggCollisionShape::Convex, ShapeIndex, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, FSkeletonTreePhysicsBodyItem::GetTypeId());
							}
						}

						// add constraints for this bone
						for (int32 ConstraintIndex = 0; ConstraintIndex < PhysicsAsset->ConstraintSetup.Num(); ++ConstraintIndex)
						{
							const FConstraintInstance& ConstraintInstance = PhysicsAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;
							if (ConstraintInstance.ConstraintBone1 == BoneName || ConstraintInstance.ConstraintBone2 == BoneName)
							{
								Output.Add(MakeShared<FSkeletonTreePhysicsConstraintItem>(PhysicsAsset->ConstraintSetup[ConstraintIndex], ConstraintIndex, BoneName, SkeletonTreePtr.Pin().ToSharedRef()), BoneName, FSkeletonTreePhysicsBodyItem::GetTypeId());
							}
						}

						break;
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE