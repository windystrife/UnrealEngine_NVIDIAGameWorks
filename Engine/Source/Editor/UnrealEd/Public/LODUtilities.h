// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Commands/UIAction.h"

//////////////////////////////////////////////////////////////////////////
// FSkeletalMeshUpdateContext


struct FSkeletalMeshUpdateContext
{
	USkeletalMesh*				SkeletalMesh;
	TArray<UActorComponent*>	AssociatedComponents;

	FExecuteAction				OnLODChanged;
};

//////////////////////////////////////////////////////////////////////////
// FLODUtilities

class UNREALED_API FLODUtilities
{
public:
	/** Removes a particular LOD from the SkeletalMesh. 
	*
	* @param UpdateContext - The skeletal mesh and actor components to operate on.
	* @param DesiredLOD   - The LOD index to remove the LOD from.
	*/
	static void RemoveLOD( FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD );

	/**
	 *	Simplifies the static mesh based upon various user settings.
	 *
	 * @param UpdateContext - The skeletal mesh and actor components to operate on.
	 * @param InSettings   - The user settings passed from the Simplification tool.
	 */
	static void SimplifySkeletalMesh( FSkeletalMeshUpdateContext& UpdateContext, TArray<FSkeletalMeshOptimizationSettings> &InSettings, bool bForceRegenerate = false );
	/**
	*	Simplifies the static mesh based upon various user settings for DesiredLOD.
	*
	* @param UpdateContext - The skeletal mesh and actor components to operate on.
	* @param Setting	- The user settings passed 
	* @param DesiredLOD - The LOD to simplify
	*/
	static void SimplifySkeletalMeshLOD(FSkeletalMeshUpdateContext& UpdateContext, const FSkeletalMeshOptimizationSettings& Setting, int32 DesiredLOD, bool bReregisterComponent = true);

	/**
	 * Refresh LOD Change
	 * 
	 * LOD has changed, it will have to notify all SMC that uses this SM
	 * and ask them to refresh LOD
	 *
	 * @param	InSkeletalMesh	SkeletalMesh that LOD has been changed for
	 */
	static void RefreshLODChange(const USkeletalMesh* SkeletalMesh);

private:
	FLODUtilities() {}

	/**
	 *	Simplifies the static mesh based upon various user settings for DesiredLOD
	 *  This is private function that gets called by SimplifySkeletalMesh
	 *
	 * @param SkeletalMesh - The skeletal mesh and actor components to operate on.
	 * @param InSetting   - The user settings passed from the Simplification tool.
	 * @param DesiredLOD - Desired LOD
	 */
	static void SimplifySkeletalMeshLOD(USkeletalMesh* SkeletalMesh, const FSkeletalMeshOptimizationSettings& InSetting, int32 DesiredLOD, bool bReregisterComponent = true);
};
