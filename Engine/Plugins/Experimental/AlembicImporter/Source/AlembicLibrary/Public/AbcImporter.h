// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "SkeletalMeshTypes.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSequence.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
	#include <Alembic/AbcGeom/All.h>
THIRD_PARTY_INCLUDES_END

class UMaterial;
class UStaticMesh;
class USkeletalMesh;
class UGeometryCache;
class UGeometryCacheTrack_FlipbookAnimation;
class UGeometryCacheTrack_TransformAnimation;
class UAbcImportSettings;
class FSkeletalMeshImportData;
class UAbcAssetImportData;

struct FAbcImportData;
struct FGeometryCacheMeshData;
struct FAbcPolyMeshObject;
struct FAbcTransformObject;
struct FAbcMeshSample;
struct FAbcCompressionSettings;
struct FCompressedAbcData;
struct FRawMesh;

enum EAbcImportError : uint32
{
	AbcImportError_NoError,
	AbcImportError_InvalidArchive,
	AbcImportError_NoValidTopObject,	
	AbcImportError_NoMeshes,
	AbcImportError_FailedToImportData
};

class ALEMBICLIBRARY_API FAbcImporter
{
public:
	FAbcImporter();
	~FAbcImporter();
public:
	/**
	* Opens and caches basic data from the Alembic file to be used for populating the importer UI
	* 
	* @param InFilePath - Path to the Alembic file to be opened
	* @return - Possible error code 
	*/
	const EAbcImportError OpenAbcFileForImport(const FString InFilePath);

	/**
	* Imports the individual tracks from the Alembic file
	*
	* @param InNumThreads - Number of threads to use for importing
	* @return - Possible error code
	*/
	const EAbcImportError ImportTrackData(const int32 InNumThreads, UAbcImportSettings* ImportSettings);
	
	/**
	* Import Alembic meshes as a StaticMeshInstance
	*	
	* @param InParent - ParentObject for the static mesh
	* @param Flags - Object flags
	* @return FStaticMesh*
	*/
	const TArray<UStaticMesh*> ImportAsStaticMesh(UObject* InParent, EObjectFlags Flags);

	/**
	* Import an Alembic file as a GeometryCache
	*	
	* @param InParent - ParentObject for the static mesh
	* @param Flags - Object flags
	* @return UGeometryCache*
	*/
	UGeometryCache* ImportAsGeometryCache(UObject* InParent, EObjectFlags Flags);
	
	TArray<UObject*> ImportAsSkeletalMesh(UObject* InParent, EObjectFlags Flags);	

	/**
	* Reimport an Alembic mesh
	*
	* @param Mesh - Current StaticMesh instance
	* @return FStaticMesh*
	*/
	const TArray<UStaticMesh*> ReimportAsStaticMesh(UStaticMesh* Mesh);

	/**
	* Reimport an Alembic file as a GeometryCache
	*
	* @param GeometryCache - Current GeometryCache instance
	* @return UGeometryCache*
	*/
	UGeometryCache* ReimportAsGeometryCache(UGeometryCache* GeometryCache);

	/**
	* Reimport an Alembic file as a SkeletalMesh
	*
	* @param SkeletalMesh - Current SkeletalMesh instance
	* @return USkeletalMesh*
	*/
	TArray<UObject*> ReimportAsSkeletalMesh(USkeletalMesh* SkeletalMesh);

	/** Returns the array of imported PolyMesh objects */
	const TArray<TSharedPtr<FAbcPolyMeshObject>>& GetPolyMeshes() const;
	
	/** Returns the number of frames for the imported Alembic file */
	const uint32 GetNumFrames() const;

	/** Returns the lowest frame index containing data for the imported Alembic file */
	const uint32 GetStartFrameIndex() const;

	/** Returns the highest frame index containing data for the imported Alembic file */
	const uint32 GetEndFrameIndex() const;

	/** Returns the number of tracks found in the imported Alembic file */
	const uint32 GetNumMeshTracks() const;


	void UpdateAssetImportData(UAbcAssetImportData* AssetImportData);
	void RetrieveAssetImportData(UAbcAssetImportData* ImportData);
private:
	/**
	* Creates an template object instance taking into account existing Instances and Objects (on reimporting)
	*
	* @param InParent - ParentObject for the geometry cache, this can be changed when parent is deleted/re-created
	* @param ObjectName - Name to be used for the created object
	* @param Flags - Object creation flags
	*/
	template<typename T> T* CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags);

	/** Recursive functionality to traverse a whole Alembic Archive and caching all the object type/data */
	void TraverseAbcHierarchy(const Alembic::Abc::IObject& InObject, TArray<TSharedPtr<FAbcTransformObject>>& InObjectHierarchy, FGuid InGuid);

	/** Templated function to parse a specific Alembic typed object from the archive */
	template<typename T> void ParseAbcObject(T& InObject, FGuid InHierarchyGuid) {};

	/**
	* CreateFlipbookAnimationTrack
	*
	* @param Name - Alembic Object Name
	* @param PolyMesh - Alembic PolyMeshSchema Object used for creation of the track
	* @param GeometryCacheParent - Parent for the GeometryCacheTrack
	* @return UGeometryCacheTrack_FlipbookAnimation*
	*/
	UGeometryCacheTrack_FlipbookAnimation* CreateFlipbookAnimationTrack(const FString& TrackName, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, UGeometryCache* GeometryCacheParent, const uint32 MaterialOffset);

	/**
	* CreateTransformAnimationTrack
	*
	* @param Name - Alembic Object Name
	* @param GeometryCacheParent - Parent for the GeometryCacheTrack
	* @return UGeometryCacheTrack_TransformAnimation*
	*/
	UGeometryCacheTrack_TransformAnimation* CreateTransformAnimationTrack(const FString& TrackName, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, UGeometryCache* GeometryCacheParent, const uint32 MaterialOffset);

	/** Generates and populates a FGeometryCacheMeshData instance from and for the given mesh object and sample index */
	void GenerateGeometryCacheMeshDataForSample(FGeometryCacheMeshData& OutMeshData, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, const uint32 SampleIndex, const uint32 MaterialOffset);
	
	/**
	* Import a single Alembic mesh as a StaticMeshInstance
	*
	* @param MeshIndex - Index into the MeshObjects Array
	* @param InParent - ParentObject for the static mesh
	* @param Flags - Object flags
	* @return UStaticMesh*
	*/
	UStaticMesh* ImportSingleAsStaticMesh(const int32 MeshIndex, UObject* InParent, EObjectFlags Flags);

	/**
	* Creates a Static mesh from the given raw mesh
	*	
	* @param InParent - ParentObject for the static mesh
	* @param Name - Name for the static mesh
	* @param Flags - Object flags
	* @param NumMaterials - Number of materials to add
	* @param FaceSetNames - Face set names used for retrieving the materials
	* @param RawMesh - The RawMesh from which the static mesh should be constructed
	* @return UStaticMesh*
	*/
	UStaticMesh* CreateStaticMeshFromRawMesh(UObject* InParent, const FString& Name, EObjectFlags Flags, const uint32 NumMaterials, const TArray<FString>& FaceSetNames, FRawMesh& RawMesh);

	/** Generates and populate a FRawMesh instance from the specific sample index */
	void GenerateRawMeshFromSample(const uint32 FirstSampleIndex, const uint32 SampleIndex, FRawMesh& RawMesh);
	/** Generates and populate a FRawMesh instance from the given sample*/
	void GenerateRawMeshFromSample(FAbcMeshSample* Sample, FRawMesh& RawMesh);
	
	/** Retrieves matrix samples using the Hierarchy linked to the given GUID */
	void GetMatrixSamplesForGUID(const FGuid& InGUID, const float StartSampleTime, const float EndSampleTime, TArray<FMatrix>& MatrixSamples, TArray<float>& SampleTimes, bool& OutConstantTransform);

	/** Temporary functionality for retrieving the object hierarchy for a given Alembic object */
	void GetHierarchyForObject(const Alembic::Abc::IObject& Object, TDoubleLinkedList<Alembic::AbcGeom::IXform>& Hierarchy);

	/** Caches the matrix transform samples for all the object hierarchies retrieved from the Alembic archive */
	void CacheHierarchyTransforms(const float StartSampleTime, const float EndSampleTime);
	
	/** Retrieves a material according to the given name and resaves it into the parent package*/
	UMaterialInterface* RetrieveMaterial(const FString& MaterialName, UObject* InParent, EObjectFlags Flags );
		
	/** Compresses the imported animation data, returns true if compression was successful and compressed data was populated */
	const bool CompressAnimationDataUsingPCA(const FAbcCompressionSettings& InCompressionSettings, const bool bRunComparison = false);	
	/** Performs the actual SVD compression to retrieve the bases and weights used to set up the Skeletal mesh's morph targets */
	const int32 PerformSVDCompression(TArray<float>& OriginalMatrix, const uint32 NumRows, const uint32 NumSamples, TArray<float>& OutU, TArray<float>& OutV, const float InPercentage, const int32 InFixedNumValue);
	/** Functionality for comparing the matrices and calculating the difference from the original animation */
	void CompareCompressionResult(const TArray<float>& OriginalMatrix, const uint32 NumSamples, const uint32 NumRows, const uint32 NumUsedSingularValues, const uint32 NumVertices, const TArray<float>& OutU, const TArray<float>& OutV, const TArray<FVector>& AverageFrame);
	
	/** Build a skeletal mesh from the PCA compressed data */
	bool BuildSkeletalMesh(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, FAbcMeshSample* Sample, TArray<int32>& OutMorphTargetVertexRemapping, TArray<int32>& OutUsedVertexIndicesForMorphs);
	
	/** Generate morph target vertices from the PCA compressed bases */
	void GenerateMorphTargetVertices(FAbcMeshSample* BaseSample, TArray<FMorphTargetDelta> &MorphDeltas, FAbcMeshSample* AverageSample, uint32 WedgeOffset, const TArray<int32>& RemapIndices, const TArray<int32>& UsedVertexIndicesForMorphs, const uint32 VertexOffset, const uint32 IndexOffset);
	
	/** Set up correct morph target weights from the PCA compressed data */
	void SetupMorphTargetCurves(USkeleton* Skeleton, FName ConstCurveName, UAnimSequence* Sequence, const TArray<float> &CurveValues, const TArray<float>& TimeValues);
	
private:
	FAbcImportData* ImportData;
	static const int32 FirstSampleIndex;
};

/** Specialized template function to parse IPolyMesh object types */
template<> void FAbcImporter::ParseAbcObject<Alembic::AbcGeom::IPolyMesh>(Alembic::AbcGeom::IPolyMesh& InPolyMesh, FGuid InHierarchyGuid);
/** Specialized template function to parse IXform object types */
template<> void FAbcImporter::ParseAbcObject<Alembic::AbcGeom::IXform>(Alembic::AbcGeom::IXform& InXform, FGuid InHierarchyGuid);
