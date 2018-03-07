// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "FbxSceneImportOptionsSkeletalMesh.generated.h"

UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UFbxSceneImportOptionsSkeletalMesh : public UObject
{
	GENERATED_UCLASS_BODY()
	

	//////////////////////////////////////////////////////////////////////////
	// Skeletal Mesh section

	/** Enable this option to update Skeleton (of the mesh)'s reference pose. Mesh's reference pose is always updated.  */
	UPROPERTY(EditAnywhere, Category = SkeletalMesh, meta = (ToolTip = "If enabled, update the Skeleton (of the mesh being imported)'s reference pose."))
	uint32 bUpdateSkeletonReferencePose : 1;

	/** If checked, create new PhysicsAsset if it doesn't have it */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = SkeletalMesh)
	uint32 bCreatePhysicsAsset : 1;

	/** TODO support T0AsRefPose Enable this option to use frame 0 as reference pose */
	UPROPERTY()
	uint32 bUseT0AsRefPose : 1;

	/** If checked, triangles with non-matching smoothing groups will be physically split. */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bPreserveSmoothingGroups : 1;

	/** If checked, meshes nested in bone hierarchies will be imported instead of being converted to bones. */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bImportMeshesInBoneHierarchy : 1;

	/** True to import morph target meshes from the FBX file */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh, meta = (ToolTip = "If enabled, creates Unreal morph objects for the imported meshes"))
	uint32 bImportMorphTargets : 1;

	/** If checked, do not filter same vertices. Keep all vertices even if they have exact same properties*/
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bKeepOverlappingVertices : 1;

	//////////////////////////////////////////////////////////////////////////
	// Animation section

	/** True to import animations from the FBX File */
	UPROPERTY(EditAnywhere, config, Category = Animation)
	uint32 bImportAnimations : 1;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, Category = Animation, config, meta = (DisplayName = "Animation Length"))
	TEnumAsByte<enum EFBXAnimationLengthImportType> AnimationLength;

	/** Frame range used when Set Range is used in Animation Length */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Animation, meta = (UIMin = 0, ClampMin = 0))
	FInt32Interval FrameImportRange;

	/** Enable this option to use default sample rate for the imported animation at 30 frames per second */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation, meta = (ToolTip = "If enabled, samples all animation curves to 30 FPS"))
	bool bUseDefaultSampleRate;

	/** Import if custom attribute as a curve within the animation **/
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bImportCustomAttribute;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bPreserveLocalTransform;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bDeleteExistingMorphTargetCurves;

	void FillSkeletalMeshInmportData(class UFbxSkeletalMeshImportData* SkeletalMeshImportData, class UFbxAnimSequenceImportData* AnimSequenceImportData, class UFbxSceneImportOptions* SceneImportOptions);
};



