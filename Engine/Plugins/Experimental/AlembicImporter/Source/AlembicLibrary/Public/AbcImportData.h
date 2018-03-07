// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Components.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>
THIRD_PARTY_INCLUDES_END

class FAbcImporter;
class UAbcImportSettings;
class UMaterial;

/** TODO
- Refactor MatrixSamples, TimeSamples into one structure
- Refactor CurveValues, TimeValues into on structure
- Optional vertex attributes, colours / visibility
*/

/** Structure for storing individual track samples */
struct FAbcMeshSample
{
	FAbcMeshSample() : NumSmoothingGroups(0)
		, NumUVSets(1)
		, NumMaterials(0)
		, SampleTime(0.0f)		
	{}

	/** Constructing from other sample*/
	FAbcMeshSample(const FAbcMeshSample& InSample)
	{
		Vertices = InSample.Vertices;
		Indices = InSample.Indices;
		Normals = InSample.Normals;
		TangentX = InSample.TangentX;
		TangentY = InSample.TangentY;
		for (uint32 UVIndex = 0; UVIndex < InSample.NumUVSets; ++UVIndex)
		{
			UVs[UVIndex] = InSample.UVs[UVIndex];
		}
		Colors = InSample.Colors;
		/*Visibility = InSample.Visibility;
		VisibilityIndices = InSample.VisibilityIndices;*/
		MaterialIndices = InSample.MaterialIndices;
		SmoothingGroupIndices = InSample.SmoothingGroupIndices;
		NumSmoothingGroups = InSample.NumSmoothingGroups;
		NumMaterials = InSample.NumMaterials;
		SampleTime = InSample.SampleTime;
		NumUVSets = InSample.NumUVSets;
	}

	TArray<FVector> Vertices;
	TArray<uint32> Indices;

	// Vertex attributes (per index based)
	TArray<FVector> Normals;
	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector2D> UVs[MAX_TEXCOORDS];	

	TArray<FLinearColor> Colors;
	/*TArray<FVector2D> Visibility;
	TArray<uint32> VisibilityIndices;*/

	// Per Face material and smoothing group index
	TArray<int32> MaterialIndices;
	TArray<uint32> SmoothingGroupIndices;

	/** Number of smoothing groups and different materials (will always be at least 1) */
	uint32 NumSmoothingGroups;
	uint32 NumUVSets;
	uint32 NumMaterials;

	// Time in track this sample was taken from
	float SampleTime;
};

/** Structure representing and Alembic poly mesh object and it's sampled data */
struct FAbcPolyMeshObject
{
	~FAbcPolyMeshObject()
	{
		for (FAbcMeshSample* Sample : MeshSamples)
		{
			if (Sample)
			{
				delete Sample;
			}
		}
	}
	/** Alembic polymesh that corresponds to this object */
	Alembic::AbcGeom::IPolyMesh Mesh;
	/** Name of this object */
	FString Name;
	/** Flag whether or not this object mesh data is constant */
	bool bConstant;
	/** Flag whether or not this object has constant topology (used for eligiblity for PCA compression) */
	bool bConstantTopology;
	/** Flag whether or not this object has a constant world matrix (used whether to incorporate into PCA compression) */
	bool bConstantTransformation;
	/** Number of samples taken for this object */
	uint32 NumSamples;
	/** Array of samples taken for this object */
	TArray<FAbcMeshSample*> MeshSamples;
	/** Array of face set names found for this object */
	TArray<FString> FaceSetNames;
	
	/** Time of first frame containing data */
	float StartFrameTime;
	/** Frame index of first frame containing data */
	uint32 StartFrameIndex;

	/** Cached self and child bounds for entire duration of the animation */
	FBoxSphereBounds SelfBounds;
	FBoxSphereBounds ChildBounds;

	/** GUID identifying the hierarchy for this object (parent structure) */
	FGuid HierarchyGuid;

	/** Flag whether or not this object should be imported (set in import UI) */
	bool bShouldImport;
};

struct FCompressedAbcData
{
	~FCompressedAbcData()
	{
		delete AverageSample;
		for (FAbcMeshSample* Sample : BaseSamples)
		{			
			delete Sample;
		}
	}
	/** GUID identifying the poly mesh object this compressed data corresponds to */
	FGuid Guid;	
	/** Average sample to apply the bases to */
	FAbcMeshSample* AverageSample;
	/** List of base samples calculated using PCA compression */
	TArray<FAbcMeshSample*> BaseSamples;
	/** Contains the curve values for each individual base */
	TArray<TArray<float>> CurveValues;
	/** Contains the time key values for each individual base */
	TArray<TArray<float>> TimeValues;
	/** Material names used for retrieving created materials */
	TArray<FString> MaterialNames;
};

/** Structure representing and Alembic transform object and it's sampled data */
struct FAbcTransformObject
{
	/** Alembic XForm this object corresponds to */
	Alembic::AbcGeom::IXform Transform;
	/** Name of this object */
	FString Name;
	/** Number of matrix samples for this object */
	uint32 NumSamples;
	/** Flag whether or not this transformation object is constant */
	bool bConstant;

	/** GUID identifying the hierarchy for this object (parent structure) */
	FGuid HierarchyGuid;

	/** Time of first frame containing data */
	float StartFrameTime;
	/** Frame index of first frame containing data */
	uint32 StartFrameIndex;

	/** Cached self and child bounds for entire duration of the animation */
	FBoxSphereBounds SelfBounds;
	FBoxSphereBounds ChildBounds;

	/** Matrix samples taken for this object */
	TArray<FMatrix> MatrixSamples;
	/** Corresponding time values for the matrix samples taken for this object */
	TArray<float> TimeSamples;
};

/** Structure used to store the cached hierarchy matrices*/
struct FCachedHierarchyTransforms
{
	TArray<FMatrix> MatrixSamples;
	TArray<float> TimeSamples;
};

/** Structure containing compressed import data for creating a skeletal mesh */
struct FAbcSkeletalMeshImportData
{
	FAbcSkeletalMeshImportData()
	{
		TotalNumMaterials = TotalNumVertices = 0;
	}

	/** Resulting compressed data from PCA compression */
	TArray<FCompressedAbcData> CompressedMeshData;
	/** Number of materials, verts and smoothing groups */
	uint32 TotalNumMaterials;
	uint32 TotalNumVertices;
	uint32 TotalNumSmoothingGroups;
};

/** Mesh section used for chunking the mesh data during Skeletal mesh building */
struct FMeshSection
{
	FMeshSection() : MaterialIndex(0), NumFaces(0) {}
	int32 MaterialIndex;
	TArray<uint32> Indices;
	TArray<uint32> OriginalIndices;
	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector> TangentZ;
	TArray<FVector2D> UVs[MAX_TEXCOORDS];
	TArray<FColor> Colors;
	uint32 NumFaces;
	uint32 NumUVSets;
};

/** Structure encapsulating all the (intermediate) data retrieved from an Alembic file by the AbcImporter*/
struct FAbcImportData
{
public:
	FAbcImportData() : NumFrames(0)
		, FramesPerSecond(0)
		, SecondsPerFrame(0.0f)
		, ArchiveBounds(EForceInit::ForceInitToZero)
		, MinTime(TNumericLimits<float>::Max())
		, MaxTime(TNumericLimits<float>::Min())
		, MinFrameIndex(TNumericLimits<uint32>::Max())
		, MaxFrameIndex(TNumericLimits<uint32>::Min())
		, NumTotalMaterials(0)
		, ImportSettings(nullptr)
		, bBackedSupportsMultithreading(false)
	{		
	}

	~FAbcImportData()
	{
		// Clear up unused materials (this could be due to reimporting, or overriding existing assets)
		for (TPair<FString, UMaterialInterface*>& Pair : MaterialMap)
		{
			if (Pair.Value != nullptr && Pair.Value->IsValidLowLevel() && Pair.Value->GetOutermost() == GetTransientPackage())
			{
				Pair.Value->MarkPendingKill();
			}
		}
	}

public:
	/** Hierarchies (parenting structure) stored for retrieving matrix samples */
	TMap<FGuid, TArray<TSharedPtr<FAbcTransformObject>>> Hierarchies;
	TMap<FGuid, TSharedPtr<FCachedHierarchyTransforms>> CachedHierarchyTransforms;

	/** Arrays of imported Alembic objects */
	TArray<TSharedPtr<FAbcPolyMeshObject>> PolyMeshObjects;
	TArray<TSharedPtr<FAbcTransformObject>> TransformObjects;

	/** Resulting compressed data from PCA compression */
	TArray<FCompressedAbcData> CompressedMeshData;
	
	/** Map of material created for the imported alembic file identified by material names */
	TMap<FString, UMaterialInterface*> MaterialMap;

	/** Total (max) number of frames in the Alembic file */
	uint32 NumFrames;
	/** Frames per second (retrieved and specified in top Alembic object) */
	uint32 FramesPerSecond;
	/** Seconds per frame (calculated according to FPS) */
	float SecondsPerFrame;

	/** Entire bounds of the archive over time */
	FBoxSphereBounds ArchiveBounds;

	/** Min and maximum time found in the Alembic file*/
	float MinTime;
	float MaxTime;
	/** Final length (in seconds)_of sequence we are importing */
	float ImportLength;

	/** Min and maximum frame index which contain actual data in the Alembic file*/
	uint32 MinFrameIndex;
	uint32 MaxFrameIndex;

	/** File path for the ABC file that is (currently being) imported */
	FString FilePath;

	/** Keeps track of total number of materials during importing, to ensure correct material indices per object */
	uint32 NumTotalMaterials;

	float ArchiveTimePerCycle;

	/** Settings (retrieved from import UI window) determining various import settings */
	UAbcImportSettings* ImportSettings;

	/** Flag to know whether or not we can run the data retrieval in parallel */
	bool bBackedSupportsMultithreading;
};
