// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RawMesh.h"
#include "StaticMeshResources.h"

typedef TPair<uint32, uint32> MeshLODPair;
typedef TPair<uint32, uint32> SectionRemapPair;
typedef TPair<uint32, uint32> MaterialRemapPair;

struct FSectionInfo;

/** Structure representing a mesh and lod index */
struct FMeshLODKey
{
public:
	FMeshLODKey(int32 MeshIndex, int32 LODIndex)
	{
		CombinedKey = ((LODIndex & 0xffff) << 16) | (MeshIndex & 0xffff);
	}

	explicit FMeshLODKey(uint32 InCombinedKey)
	{
		CombinedKey = InCombinedKey;
	}

	int32 GetMeshIndex() const
	{
		return (CombinedKey & 0x0000ffff);
	}

	int32 GetLODIndex() const
	{
		return (CombinedKey & 0xffff0000) >> 16;
	}

	inline bool operator==(const FMeshLODKey& Other)
	{
		return CombinedKey == Other.CombinedKey;
	}

	inline friend bool operator==(const FMeshLODKey& Lhs, const FMeshLODKey& Rhs)
	{
		return Lhs.CombinedKey == Rhs.CombinedKey;
	}

	inline friend uint32 GetTypeHash(const FMeshLODKey& Item)
	{
		return Item.CombinedKey;
	}

protected:
	uint32 CombinedKey;
};

/** Typedefs to allow for some nicer looking loops */
typedef TMap<FMeshLODKey, FRawMesh>::TConstIterator TConstRawMeshIterator;
typedef TMap<FMeshLODKey, FRawMesh>::TIterator TRawMeshIterator;

typedef TArray<int32>::TConstIterator TConstLODIndexIterator;

/** Used to keep track of in-flight data while meshes are merged and their corresponding materials baked down */
class FMeshMergeDataTracker
{	
public:
	FMeshMergeDataTracker();

	/** Looks at all available raw mesh data and processes it to populate some flags */
	void ProcessRawMeshes();

	/** Adding a mapping between the index for an original mesh section index and the mesh section it will be indexed to in the final mesh */
	void AddSectionRemapping(int32 MeshIndex, int32 LODIndex, int32 OriginalIndex, int32 UniqueIndex);
	/** Retrieves the MeshLOD keys from which the original sections are mapped to the unique section index */
	void GetMeshLODsMappedToUniqueSection(int32 UniqueIndex, TArray<FMeshLODKey>& InOutMeshLODs);
	/** Retrieves all section mappings for the MeshLOD key*/
	void GetMappingsForMeshLOD(FMeshLODKey Key, TArray<SectionRemapPair>& InOutMappings);
	
	/** Adds or retrieves raw mesh data for the mesh and LOD index */
	FRawMesh& AddAndRetrieveRawMesh(int32 MeshIndex, int32 LODIndex);
	/** Removes raw mesh entry for the given mesh and LOD index */
	void RemoveRawMesh(int32 MeshIndex, int32 LODIndex);
	/** Retrieves Raw Mesh ptr for the given mesh and LOD index */
	FRawMesh* GetRawMeshPtr(int32 MeshIndex, int32 LODIndex);
	/** Retrieves Raw Mesh ptr for the given MeshLOD key */
	FRawMesh* GetRawMeshPtr(FMeshLODKey Key);
	/** Tries to retrieve a FRawMesh and returns the LOD index it found an entry for */
	FRawMesh* FindRawMeshAndLODIndex(int32 MeshIndex, int32& OutLODIndex);
	/** Tries to retrieve a FRawMesh for the given mesh and LOD index, if it can't it will try to find an entry for each LOD levle below InOutDesiredLODIndex */
	FRawMesh* TryFindRawMeshForLOD(int32 MeshIndex, int32& InOutDesiredLODIndex);

	/** Returns a const key/value iterator for the FRawMesh entries */
	TConstRawMeshIterator GetConstRawMeshIterator() const;
	/** Returns a non-const key/value iterator for the FRawMesh entries */ 
	TRawMeshIterator GetRawMeshIterator();

	/** Adds a record of what channel lightmap data is stored at */
	void AddLightmapChannelRecord(int32 MeshIndex, int32 LODIndex, int32 LightmapChannelIndex);

	/** Adds (unique) section to stored data */
	int32 AddSection(const FSectionInfo& SectionInfo);
	/** Returns the number of unique sections */
	int32 NumberOfUniqueSections() const;
	/** Returns the material used by the unique section */
	UMaterialInterface* GetMaterialForSectionIndex(int32 SectionIndex);
	/** Returns the unique section instance */
	const FSectionInfo& GetSection(int32 SectionIndex) const;
	/** Clears out unique section to be replaced with the baked material one */
	void AddBakedMaterialSection(const FSectionInfo& SectionInfo);
	
	/** Add a material slot name for a unique material instance. */
	void AddMaterialSlotName(UMaterialInterface *MaterialInterface, FName MaterialSlotName);
	/** Get the material slot name from a unique material instance. */
	FName GetMaterialSlotName(UMaterialInterface *MaterialInterface) const;

	/** Adds a LOD index which will be part of the final merged mesh */
	void AddLODIndex(int32 LODIndex);
	/** Retrieves number of LODs part of the final merged mesh */
	int32 GetNumLODsForMergedMesh() const;
	/** Iterates over LOD indices for mesh */
	TConstLODIndexIterator GetLODIndexIterator() const;

	/** Add number of lightmap pixels used for one of the Meshes */
	void AddLightMapPixels(int32 Pixels);
	/** Returns the texture dimension required to distribute all of the lightmap pixels */
	int32 GetLightMapDimension() const;

	/** Returns whether or not any raw mesh entry contains vertex colors for the specified LOD index */
	bool DoesLODContainVertexColors(int32 LODIndex) const;
	/** Returns whether or not any raw mesh entry contains texture coordinates for the specified UV channel and LOD index */
	bool DoesUVChannelContainData(int32 UVChannel, int32 LODIndex) const;
	/** Returns whether or not the raw mesh entry for the given MeshLOD key requires unique UVs for baking out its material(s) */
	bool DoesMeshLODRequireUniqueUVs(FMeshLODKey Key);
	/** Returns the first available UV channel across all raw mesh entries, which will be a good fit for the lightmap UV index in the final mesh */
	int32 GetAvailableLightMapUVChannel() const;

protected:
	// Mesh / LOD index, RawMesh
	TMap<FMeshLODKey, FRawMesh> RawMeshLODs;

	// Mesh / LOD index, lightmap channel
	TMap<FMeshLODKey, int32> LightmapChannelLODs;

	// Whether a key requires unique UVs 
	TArray<FMeshLODKey> RequiresUniqueUVs;

	//Use this map to recycle the material slot name
	TMap<UMaterialInterface *, FName> MaterialInterfaceToMaterialSlotName;
	
	/** Flags for UV and vertex color usage */
	bool bWithVertexColors[MAX_STATIC_MESH_LODS];
	bool bOcuppiedUVChannels[MAX_STATIC_MESH_LODS][MAX_MESH_TEXTURE_COORDS];
	/** First available UV channel across all RawMesh entries */
	int32 AvailableLightMapUVChannel;
	int32 SummedLightMapPixels;

	/** Remapping pairs for each mesh and LOD index combination */
	TMultiMap<FMeshLODKey, SectionRemapPair> UniqueSectionIndexPerLOD;
	/** Maps from each unique section index to all the RawMesh entries which contain an original section that's mapped to it */
	TMultiMap<uint32, FMeshLODKey> UniqueSectionToMeshLOD;

	/** All LOD indices which should be populated in the final merged mesh */
	TArray<int32> LODIndices;
	
	/** Unique set of sections in mesh */
	TArray<FSectionInfo> UniqueSections;
};

