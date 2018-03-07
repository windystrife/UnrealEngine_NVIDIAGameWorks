// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/ShapeElem.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.generated.h"

class FPrimitiveDrawInterface;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class UPhysicsAssetEditorSkeletalMeshComponent : public UDebugSkelMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Data and methods shared across multiple classes */
	class FPhysicsAssetEditorSharedData* SharedData;

	// Draw colors

	FColor BoneUnselectedColor;
	FColor NoCollisionColor;
	FColor FixedColor;
	FColor ConstraintBone1Color;
	FColor ConstraintBone2Color;
	FColor HierarchyDrawColor;
	FColor AnimSkelDrawColor;
	float COMRenderSize;
	float InfluenceLineLength;
	FColor InfluenceLineColor;

	// Materials

	UPROPERTY(transient)
	UMaterialInstanceDynamic* ElemSelectedMaterial;
	UPROPERTY(transient)
	UMaterialInstanceDynamic* BoneSelectedMaterial;
	UPROPERTY(transient)
	UMaterialInstanceDynamic* BoneUnselectedMaterial;
	UPROPERTY(transient)
	UMaterialInterface* BoneMaterialHit;
	UPROPERTY(transient)
	UMaterialInstanceDynamic* BoneNoCollisionMaterial;

	/** Mesh-space matrices showing state of just animation (ie before physics) - useful for debugging! */
	TArray<FTransform> AnimationSpaceBases;


	/** UPrimitiveComponent interface */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** USkinnedMeshComponent interface */
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = nullptr) override;
	
	/** Debug drawing */
	void DebugDraw(const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Handles most of the rendering logic for this component */
	void RenderAssetTools(const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Draws a constraint */
	void DrawConstraint(int32 ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bDrawAsPoint);

	/** Accessors/helper methods */
	FTransform GetPrimitiveTransform(FTransform& BoneTM, int32 BodyIndex, EAggCollisionShape::Type PrimType, int32 PrimIndex, float Scale);
	FColor GetPrimitiveColor(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType, int32 PrimitiveIndex);
	UMaterialInterface* GetPrimitiveMaterial(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType, int32 PrimitiveIndex);
};
