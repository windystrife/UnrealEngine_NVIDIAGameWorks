// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "BoneIndices.h"

class USkeletalMesh;
struct FSkelMeshSection;

/**
 * Mesh reduction interface.
 */
class IMeshBoneReduction
{
public:
	/**
	 * Fix up section bone maps based on list of bones to remove
	 * List of bones to remove should contains <bone index to remove, bone index to replace to >
	 * 
	 * @param	Section : Section reference to fix up
	 * @param	BonesToRemove : List of bones to remove with a pair of [bone index, bone to replace]
	 *
	 * @return	true if success 
	 */
	virtual void FixUpSectionBoneMaps( FSkelMeshSection & Section, const TMap<FBoneIndexType, FBoneIndexType> &BonesToRepair ) = 0;
	/**
	 * Get Bones To Remove from the Desired LOD
	 * List of bones to remove should contains <bone index to remove, bone index to replace to >
	 *
	 * @param	SkeletalMesh : SkeletalMesh to test
	 * @param	DesiredLOD	: 0 isn't valid as this will only test from [LOD 1, LOD (N-1)] since Skeleton doesn't save any bones to remove setting on based LOD
	 * @param	OutBonesToReplace : List of bones to replace with a pair of [bone index, bone index to replace to]
	 *
	 * @return	true if any bone to be replaced
	 */
	virtual bool GetBoneReductionData(const USkeletalMesh* SkeletalMesh, int32 DesiredLOD, TMap<FBoneIndexType, FBoneIndexType> &OutBonesToReplace, const TArray<FName>* BoneNamesToRemove = NULL) = 0;

	/**
	 * Reduce Bone Counts for the SkeletalMesh with the LOD
	 *
	 * @param	SkeletalMesh : SkeletalMesh
	 * @param	DesriedLOD	: The data to reduce comes from Skeleton 
	 *
	 */
	virtual bool ReduceBoneCounts(USkeletalMesh * SkeletalMesh, int32 DesiredLOD, const TArray<FName>* BoneNamesToRemove) = 0;
};

/**
 * Mesh reduction module interface.
 */
class IMeshBoneReductionModule : public IModuleInterface
{
public:
	/**
	 * Retrieve the mesh reduction interface.
	 */
	virtual class IMeshBoneReduction* GetMeshBoneReductionInterface() = 0;
};
