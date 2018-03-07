// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Preferences/PhysicsAssetEditorOptions.h"
#include "SceneManagement.h"
#include "PhysicsAssetEditorSharedData.h"
#include "PhysicsAssetEditorHitProxies.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "SkeletalMeshTypes.h"
#include "UObject/Package.h"
#include "EditorStyleSet.h"

namespace
{
	// How large to make the constraint arrows.
	// The factor of 60 was found experimentally, to look reasonable in comparison with the rest of the constraint visuals.
	constexpr float ConstraintArrowScale = 60.0f;
}

UPhysicsAssetEditorSkeletalMeshComponent::UPhysicsAssetEditorSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BoneUnselectedColor(170,155,225)
	, NoCollisionColor(200, 200, 200)
	, FixedColor(125,125,0)
	, ConstraintBone1Color(255,166,0)
	, ConstraintBone2Color(0,150,150)
	, HierarchyDrawColor(220, 255, 220)
	, AnimSkelDrawColor(255, 64, 64)
	, COMRenderSize(5.0f)
	, InfluenceLineLength(2.0f)
	, InfluenceLineColor(0,255,0)
{

	// Body materials
	UMaterialInterface* BaseElemSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_ElemSelectedMaterial.PhAT_ElemSelectedMaterial"), NULL, LOAD_None, NULL);
	ElemSelectedMaterial = UMaterialInstanceDynamic::Create(BaseElemSelectedMaterial, GetTransientPackage());
	check(ElemSelectedMaterial);

	UMaterialInterface* BaseBoneSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_BoneSelectedMaterial.PhAT_BoneSelectedMaterial"), NULL, LOAD_None, NULL);
	BoneSelectedMaterial = UMaterialInstanceDynamic::Create(BaseBoneSelectedMaterial, GetTransientPackage());
	check(BoneSelectedMaterial);

	BoneMaterialHit = UMaterial::GetDefaultMaterial(MD_Surface);
	check(BoneMaterialHit);

	UMaterialInterface* BaseBoneUnselectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_UnselectedMaterial.PhAT_UnselectedMaterial"), NULL, LOAD_None, NULL);
	BoneUnselectedMaterial = UMaterialInstanceDynamic::Create(BaseBoneUnselectedMaterial, GetTransientPackage());
	check(BoneUnselectedMaterial);

	UMaterialInterface* BaseBoneNoCollisionMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_NoCollisionMaterial.PhAT_NoCollisionMaterial"), NULL, LOAD_None, NULL);
	BoneNoCollisionMaterial = UMaterialInstanceDynamic::Create(BaseBoneNoCollisionMaterial, GetTransientPackage());
	check(BoneNoCollisionMaterial);

	// this is because in phat editor, you'd like to see fixed bones to be fixed without animation force update
	KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
	bUpdateJointsFromAnimation = false;
	ForcedLodModel = 1;

	static FName CollisionProfileName(TEXT("PhysicsActor"));
	SetCollisionProfileName(CollisionProfileName);

	bSelectable = false;
}

void UPhysicsAssetEditorSkeletalMeshComponent::RenderAssetTools(const FSceneView* View, class FPrimitiveDrawInterface* PDI)
{
	check(SharedData);

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	
	if(!PhysicsAsset)
	{
		// Nothing to draw without an asset, this can happen if the preview scene has no skeletal mesh
		return;
	}

	EPhysicsAssetEditorRenderMode CollisionViewMode = SharedData->GetCurrentCollisionViewMode(SharedData->bRunningSimulation);

#if DEBUG_CLICK_VIEWPORT
	PDI->DrawLine(SharedData->LastClickOrigin, SharedData->LastClickOrigin + SharedData->LastClickDirection * 5000.0f, FLinearColor(1, 1, 0, 1), SDPG_Foreground);
	PDI->DrawPoint(SharedData->LastClickOrigin, FLinearColor(1, 0, 0), 5, SDPG_Foreground);
#endif

	// set opacity of our materials
	static FName OpacityName(TEXT("Opacity"));
	ElemSelectedMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->CollisionOpacity);
	BoneSelectedMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->CollisionOpacity);
	BoneUnselectedMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->bSolidRenderingForSelectedOnly ? 0.0f : SharedData->EditorOptions->CollisionOpacity);
	BoneNoCollisionMaterial->SetScalarParameterValue(OpacityName, SharedData->EditorOptions->bSolidRenderingForSelectedOnly ? 0.0f : SharedData->EditorOptions->CollisionOpacity);

	static FName SelectionColorName(TEXT("SelectionColor"));
	const FSlateColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName);
	const FLinearColor LinearSelectionColor( SelectionColor.IsColorSpecified() ? SelectionColor.GetSpecifiedColor() : FLinearColor::White );

	ElemSelectedMaterial->SetVectorParameterValue(SelectionColorName, LinearSelectionColor);
	BoneSelectedMaterial->SetVectorParameterValue(SelectionColorName, LinearSelectionColor);

	// Draw bodies
	for (int32 i = 0; i <PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		int32 BoneIndex = GetBoneIndex(PhysicsAsset->SkeletalBodySetups[i]->BoneName);

		// If we found a bone for it, draw the collision.
		// The logic is as follows; always render in the ViewMode requested when not in hit mode - but if we are in hit mode and the right editing mode, render as solid
		if (BoneIndex != INDEX_NONE)
		{
			FTransform BoneTM = GetBoneTransform(BoneIndex);
			float Scale = BoneTM.GetScale3D().GetAbsMax();
			FVector VectorScale(Scale);
			BoneTM.RemoveScaling();

			FKAggregateGeom* AggGeom = &PhysicsAsset->SkeletalBodySetups[i]->AggGeom;

			for (int32 j = 0; j <AggGeom->SphereElems.Num(); ++j)
			{
				PDI->SetHitProxy(new HPhysicsAssetEditorEdBoneProxy(i, EAggCollisionShape::Sphere, j));

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, EAggCollisionShape::Sphere, j, Scale);


				//solids are drawn if it's the ViewMode and we're not doing a hit, or if it's hitAndBodyMode
				if(CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, EAggCollisionShape::Sphere, j);
					AggGeom->SphereElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid || CollisionViewMode == EPhysicsAssetEditorRenderMode::Wireframe)
				{
					AggGeom->SphereElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, EAggCollisionShape::Sphere, j));
				}

				PDI->SetHitProxy(NULL);
			}

			for (int32 j = 0; j <AggGeom->BoxElems.Num(); ++j)
			{
				PDI->SetHitProxy(new HPhysicsAssetEditorEdBoneProxy(i, EAggCollisionShape::Box, j));

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, EAggCollisionShape::Box, j, Scale);

				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, EAggCollisionShape::Box, j);
					AggGeom->BoxElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid || CollisionViewMode == EPhysicsAssetEditorRenderMode::Wireframe)
				{
					AggGeom->BoxElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, EAggCollisionShape::Box, j));
				}
				
				PDI->SetHitProxy(NULL);
			}

			for (int32 j = 0; j <AggGeom->SphylElems.Num(); ++j)
			{
				PDI->SetHitProxy(new HPhysicsAssetEditorEdBoneProxy(i, EAggCollisionShape::Sphyl, j));

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, EAggCollisionShape::Sphyl, j, Scale);

				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, EAggCollisionShape::Sphyl, j);
					AggGeom->SphylElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid || CollisionViewMode == EPhysicsAssetEditorRenderMode::Wireframe)
				{
					AggGeom->SphylElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, EAggCollisionShape::Sphyl, j));
				}
					
				PDI->SetHitProxy(NULL);
			}

			for (int32 j = 0; j <AggGeom->ConvexElems.Num(); ++j)
			{
				PDI->SetHitProxy(new HPhysicsAssetEditorEdBoneProxy(i, EAggCollisionShape::Convex, j));

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, EAggCollisionShape::Convex, j, Scale);

				//convex doesn't have solid draw so render lines if we're in hitTestAndBodyMode
				if (CollisionViewMode == EPhysicsAssetEditorRenderMode::Solid || CollisionViewMode == EPhysicsAssetEditorRenderMode::Wireframe)
				{
					AggGeom->ConvexElems[j].DrawElemWire(PDI, ElemTM, Scale, GetPrimitiveColor(i, EAggCollisionShape::Convex, j));
				}
				
				PDI->SetHitProxy(NULL);
			}

			if (SharedData->bShowCOM && Bodies.IsValidIndex(i))
			{
				Bodies[i]->DrawCOMPosition(PDI, COMRenderSize, SharedData->COMRenderColor);
			}
		}
	}

	// Draw Constraints
	EPhysicsAssetEditorConstraintViewMode ConstraintViewMode = SharedData->GetCurrentConstraintViewMode(SharedData->bRunningSimulation);
	if (ConstraintViewMode != EPhysicsAssetEditorConstraintViewMode::None)
	{
		for (int32 i = 0; i <PhysicsAsset->ConstraintSetup.Num(); ++i)
		{
			int32 BoneIndex1 = GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone1);
			int32 BoneIndex2 = GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone2);
			// if bone doesn't exist, do not draw it. It crashes in random points when we try to manipulate. 
			if (BoneIndex1 != INDEX_NONE && BoneIndex2 != INDEX_NONE)
			{
				PDI->SetHitProxy(new HPhysicsAssetEditorEdConstraintProxy(i));

				DrawConstraint(i, View, PDI, SharedData->EditorOptions->bShowConstraintsAsPoints);

				PDI->SetHitProxy(NULL);
			}
		}
	}
}

void UPhysicsAssetEditorSkeletalMeshComponent::DebugDraw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	RenderAssetTools(View, PDI);
}

FPrimitiveSceneProxy* UPhysicsAssetEditorSkeletalMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	EPhysicsAssetEditorRenderMode MeshViewMode = SharedData->GetCurrentMeshViewMode(SharedData->bRunningSimulation);
	if (MeshViewMode != EPhysicsAssetEditorRenderMode::None)
	{
		Proxy = UDebugSkelMeshComponent::CreateSceneProxy();
	}

	return Proxy;
}

bool ConstraintInSelected(int32 Index, const TArray<FPhysicsAssetEditorSharedData::FSelection> & Constraints)
{
	for(int32 i=0; i<Constraints.Num(); ++i)
	{

		if(Constraints[i].Index == Index)
		{
			return true;
		}
	}

	return false;
}

void UPhysicsAssetEditorSkeletalMeshComponent::DrawConstraint(int32 ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bDrawAsPoint)
{
	EPhysicsAssetEditorConstraintViewMode ConstraintViewMode = SharedData->GetCurrentConstraintViewMode(SharedData->bRunningSimulation);
	bool bDrawLimits = false;
	bool bConstraintSelected = ConstraintInSelected(ConstraintIndex, SharedData->SelectedConstraints);
	if (ConstraintViewMode == EPhysicsAssetEditorConstraintViewMode::AllLimits || bConstraintSelected)
	{
		bDrawLimits = true;
	}

	bool bDrawSelected = false;
	if (!SharedData->bRunningSimulation && bConstraintSelected)
	{
		bDrawSelected = true;
	}

	UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[ConstraintIndex];

	FTransform Con1Frame = SharedData->GetConstraintMatrix(ConstraintIndex, EConstraintFrame::Frame1, 1.f);
	FTransform Con2Frame = SharedData->GetConstraintMatrix(ConstraintIndex, EConstraintFrame::Frame2, 1.f);

	const float DrawScale = ConstraintArrowScale * SharedData->EditorOptions->ConstraintDrawSize;

	ConstraintSetup->DefaultInstance.DrawConstraint(PDI, SharedData->EditorOptions->ConstraintDrawSize, DrawScale, bDrawLimits, bDrawSelected, Con1Frame, Con2Frame, bDrawAsPoint);
}

FTransform UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveTransform(FTransform& BoneTM, int32 BodyIndex, EAggCollisionShape::Type PrimType, int32 PrimIndex, float Scale)
{
	UBodySetup* SharedBodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex];
	FVector Scale3D(Scale);

	FTransform ManTM = FTransform::Identity;

	if(SharedData->bManipulating && !SharedData->bRunningSimulation)
	{
		FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimType, PrimIndex);
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			if (Body == SharedData->SelectedBodies[i])
			{
				ManTM = SharedData->SelectedBodies[i].ManipulateTM;
				break;
			}

		}
	}


	if (PrimType == EAggCollisionShape::Sphere)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.SphereElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Box)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.BoxElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Sphyl)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.SphylElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == EAggCollisionShape::Convex)
	{
		FTransform PrimTM = ManTM * SharedBodySetup->AggGeom.ConvexElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}

	// Should never reach here
	check(0);
	return FTransform::Identity;
}

FColor UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveColor(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType, int32 PrimitiveIndex)
{
	UBodySetup* SharedBodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[ BodyIndex ];

	if (!SharedData->bRunningSimulation && SharedData->GetSelectedConstraint())
	{
		UPhysicsConstraintTemplate* cs = SharedData->PhysicsAsset->ConstraintSetup[ SharedData->GetSelectedConstraint()->Index ];

		if (cs->DefaultInstance.ConstraintBone1 == SharedBodySetup->BoneName)
		{
			return ConstraintBone1Color;
		}
		else if (cs->DefaultInstance.ConstraintBone2 == SharedBodySetup->BoneName)
		{
			return ConstraintBone2Color;
		}
	}

	FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	static FName SelectionColorName(TEXT("SelectionColor"));
	const FSlateColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName);
	const FLinearColor SelectionColorLinear(SelectionColor.IsColorSpecified() ? SelectionColor.GetSpecifiedColor() : FLinearColor::White);
	const FColor ElemSelectedColor = SelectionColorLinear.ToFColor(true);
	const FColor ElemSelectedBodyColor = (SelectionColorLinear* 0.5f).ToFColor(true);

	bool bInBody = false;
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		if(BodyIndex == SharedData->SelectedBodies[i].Index)
		{
			bInBody = true;
		}

		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return ElemSelectedColor;
		}
	}

	if(bInBody && !SharedData->bRunningSimulation)	//this primitive is in a body that's currently selected, but this primitive itself isn't selected
	{
		return ElemSelectedBodyColor;
	}
	
	if (SharedData->bRunningSimulation)
	{
		const bool bIsSimulatedAtAll = SharedBodySetup->PhysicsType == PhysType_Simulated || (SharedBodySetup->PhysicsType == PhysType_Default && SharedData->EditorOptions->PhysicsBlend > 0.f);
		if (!bIsSimulatedAtAll)
		{
			return FixedColor;
		}
	}
	else
	{
		if (!SharedData->bRunningSimulation && SharedData->SelectedBodies.Num())
		{
			// If there is no collision with this body, use 'no collision material'.
			if (SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE)
			{
				return NoCollisionColor;
			}
		}
	}

	return BoneUnselectedColor;
}

UMaterialInterface* UPhysicsAssetEditorSkeletalMeshComponent::GetPrimitiveMaterial(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType, int32 PrimitiveIndex)
{
	if (SharedData->bRunningSimulation)
	{
		return BoneUnselectedMaterial;
	}

	FPhysicsAssetEditorSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	for(int32 i=0; i< SharedData->SelectedBodies.Num(); ++i)
	{
		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return ElemSelectedMaterial;
		}
	}

	// If there is no collision with this body, use 'no collision material'.
	if (SharedData->SelectedBodies.Num() && SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE && !SharedData->bRunningSimulation)
	{
		return BoneNoCollisionMaterial;
	}
	else
	{
		return BoneUnselectedMaterial;
	}

}

void UPhysicsAssetEditorSkeletalMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	Super::RefreshBoneTransforms(TickFunction);

	// Horrible kludge, but we need to flip the buffer back here as we need to wait on the physics tick group.
	// However UDebugSkelMeshComponent passes NULL to force non-threaded work, which assumes a flip is needed straight away
	if(ShouldBlendPhysicsBones())
	{
		bNeedToFlipSpaceBaseBuffers = true;
		FinalizeBoneTransform();
		bNeedToFlipSpaceBaseBuffers = true;
	}
}