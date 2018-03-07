// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/IntPoint.h"
#include "SceneTypes.h"
#include "LightMap.h"

class UMaterialInterface;
struct FRawMesh;

/** Structure containing information about the material which is being baked out */
struct FMaterialData
{
	FMaterialData()
		: Material(nullptr)
	{}

	/** Material to bake out */
	UMaterialInterface* Material;
	/** Properties and the texture size at which they should be baked out */
	TMap<EMaterialProperty, FIntPoint> PropertySizes;
};

struct FMeshData
{
	FMeshData()
		: RawMesh(nullptr), TextureCoordinateIndex(0), LightMapIndex(0), LightMap(nullptr)
	{}

	/** Ptr to raw mesh data to use for baking out the material data, if nullptr a standard quad is used */
	FRawMesh* RawMesh;

	/** Material indices to test the Raw Mesh data against, ensuring we only bake out triangles which use the currently baked out material */
	TArray<int32> MaterialIndices;

	/** Set of custom texture coordinates which ensure that the material is baked out with unique/non-overlapping positions */
	TArray<FVector2D> CustomTextureCoordinates;

	/** Box which's space contains the UV coordinates used to bake out the material */
	FBox2D TextureCoordinateBox;
	
	/** Specific texture coordinate index to use as texture coordinates to bake out the material (is overruled if CustomTextureCoordinates contains any data) */
	int32 TextureCoordinateIndex;

	/** Light map index used to retrieve the light-map UVs from RawMesh */
	int32 LightMapIndex;

	/** Reference to the lightmap texture part of the level in the currently being baked out mesh instance data is resident */
	FLightMapRef LightMap;
};

/** Structure containing data being processed while baking out materials*/
struct FBakeOutput
{
	FBakeOutput()
		: EmissiveScale(1.0f)
	{}

	/** Contains the resulting texture data for baking out a material's property */
	TMap<EMaterialProperty, TArray<FColor>> PropertyData;

	/** Contains the resulting texture size for baking out a material's property */
	TMap<EMaterialProperty, FIntPoint> PropertySizes;

	/** Scale used to allow having wide ranges of emissive values in the source materials, the final proxy material will use this value to scale the emissive texture's pixel values */
	float EmissiveScale;
};
