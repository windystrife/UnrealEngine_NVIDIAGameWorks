// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/BoxSphereBounds.h"

struct FRawMesh;
struct FSectionInfo;
struct FMeshData;
class UMaterialInterface;

class IMaterialBakingAdapter
{
public:
	/** Returns the number of LODs for the data the adapter represents */
	virtual int32 GetNumberOfLODs() const = 0;
	/** Retrieves model data in FRawMesh form */
	virtual void RetrieveRawMeshData(int32 LODIndex, FRawMesh& InOutRawMesh, bool bPropagateMeshData) const = 0;
	/** Retrieves all mesh sections from underlying data */
	virtual void RetrieveMeshSections(int32 LODIndex, TArray<FSectionInfo>& InOutSectionInfo) const = 0;
	/** Returns lightmap UV index used by mesh data */
	virtual int32 LightmapUVIndex() const = 0;

	/** Sets material index to specified material value */
	virtual void SetMaterial(int32 MaterialIndex, UMaterialInterface* Material) = 0;
	/** Returns the material index for the given lod and section index */
	virtual int32 GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const = 0;
	/** Remaps the material index for the given lod and section index to the specified new one */
	virtual void RemapMaterialIndex(int32 LODIndex, int32 SectionIndex, int32 NewMaterialIndex) = 0;
	/** Adds a new material to the underlying asset/data */
	virtual int32 AddMaterial(UMaterialInterface* Material) = 0;

	/** Update UV channel data on object the adapter represents */
	virtual void UpdateUVChannelData() = 0;

	/** Ability to aplly custom settings to the FMeshData structure */
	virtual void ApplySettings(int32 LODIndex, FMeshData& InOutMeshData) const = 0;

	/** Returns bounds of underlying data */
	virtual FBoxSphereBounds GetBounds() const = 0;
	/** Returns outer package to use when creating new assets */
	virtual UPackage* GetOuter() const = 0;
	/** Returns base name to use for newly created assets */
	virtual FString GetBaseName() const = 0;
	/** Returns whether or not the underlying data is an UAsset */
	virtual bool IsAsset() const = 0;
};