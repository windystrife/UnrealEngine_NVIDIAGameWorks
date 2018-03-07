// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFbxImportUI;
class UStaticMesh;
class USkeletalMesh;
class UObject;

namespace FbxMeshUtils
{
	/**
	 * Imports a mesh LOD to the given static mesh
	 *
	 * @param BaseStaticMesh	The static mesh to import the LOD to
	 * @param Filename			The filename of the FBX file containing the LOD
	 * @param LODLevel			The level of the lod to import
	 * @return					Whether or not the import succeeded
	 */
	UNREALED_API bool ImportStaticMeshLOD( UStaticMesh* BaseStaticMesh, const FString& Filename, int32 LODLevel );

	/**
	 * Imports a skeletal mesh LOD to the given skeletal mesh
	 *
	 * @param Mesh				The skeletal mesh to import the LOD to
	 * @param Filename			The filename of the FBX file containing the LOD
	 * @param LODLevel			The level of the lod to import
	 * @return					Whether or not the import succeeded
	 */
	UNREALED_API bool ImportSkeletalMeshLOD( USkeletalMesh* Mesh, const FString& Filename, int32 LODLevel );

	/**
	 * Imports a skeletal mesh LOD to the given skeletal mesh
	 * Opens file dialog to do so!
	 *
	 * @param Mesh				The skeletal mesh to import the LOD to
	 * @param LODLevel			The level of the lod to import
	 * @return					Whether or not the import succeeded
	 */
	UNREALED_API bool ImportMeshLODDialog( UObject* Mesh, int32 LODLevel);

	/**
	 * Sets import option before importing
	 *
	 * @param ImportUI			The importUI you'd like to apply to
	 */
	UNREALED_API void SetImportOption(UFbxImportUI* ImportUI);

}  //end namespace ExportMeshUtils
