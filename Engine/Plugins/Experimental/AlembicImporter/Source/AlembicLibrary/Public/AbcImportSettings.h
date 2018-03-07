// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbcImportSettings.generated.h"

/** Enum that describes type of asset to import */
UENUM()
enum class EAlembicImportType : uint8
{
	/** Imports only the first frame as one or multiple static meshes */
	StaticMesh,
	/** Imports the Alembic file as flipbook and matrix animated objects */
	GeometryCache UMETA(DisplayName = "Geometry Cache (Experimental)"),
	/** Imports the Alembic file as a skeletal mesh containing base poses as morph targets and blending between them to achieve the correct animation frame */
	Skeletal
};

UENUM()
enum class EBaseCalculationType : uint8
{
	/** Determines the number of bases that should be used with the given percentage*/
	PercentageBased = 1,
	/** Set a fixed number of bases to import*/
	FixedNumber
};

USTRUCT()
struct FAbcCompressionSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcCompressionSettings()
	{		
		bMergeMeshes = false;
		bBakeMatrixAnimation = true;

		BaseCalculationType = EBaseCalculationType::PercentageBased;
		PercentageOfTotalBases = 100.0f;
		MaxNumberOfBases = 0;
		MinimumNumberOfVertexInfluencePercentage = 0.0f;
		//bOptimizeCurves = true; TODO
	}

	/** Whether or not the individual meshes should be merged for compression purposes */
	UPROPERTY(EditAnywhere, Category = Compression)
	bool bMergeMeshes;

	/** Whether or not Matrix-only animation should be baked out as vertex animation (or skipped?)*/
	UPROPERTY(EditAnywhere, Category = Compression)
	bool bBakeMatrixAnimation;
	
	/** Determines how the final number of bases that are stored as morph targets are calculated*/
	UPROPERTY(EditAnywhere, Category = Compression)
	EBaseCalculationType BaseCalculationType;

	/** Will generate given percentage of the given bases as morph targets*/
	UPROPERTY(EditAnywhere, Category = Compression, meta = (EnumCondition = 1, ClampMin = "1.0", ClampMax="100.0"))
	float PercentageOfTotalBases;

	/** Will generate given fixed number of bases as morph targets*/
	UPROPERTY(EditAnywhere, Category = Compression, meta = (EnumCondition = 2, ClampMin = "1"))
	int32 MaxNumberOfBases;

	/** Minimum percentage of influenced vertices required for a morph target to be valid */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Compression, meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MinimumNumberOfVertexInfluencePercentage;

	// TODO add optimization steps for curve keys (remove constant and resample to new keys while preserving deltas)
	/*UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Compression, meta = (ClampMin = "0"))
	bool bOptimizeCurves;*/
};

UENUM()
enum class EAlembicSamplingType : uint8
{
	/** Samples the animation according to the imported data (default)*/
	PerFrame,	
	/** Samples the animation at given intervals determined by Frame Steps*/
	PerXFrames UMETA(DisplayName = "Per X Frames"),
	/** Samples the animation at given intervals determined by Time Steps*/
	PerTimeStep
};

USTRUCT()
struct FAbcSamplingSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcSamplingSettings()
	{
		SamplingType = EAlembicSamplingType::PerFrame;
		FrameSteps = 1;
		TimeSteps = 0.0f;
		FrameStart = FrameEnd = 0;
	}

	/** Type of sampling performed while importing the animation*/
	UPROPERTY(EditAnywhere, Category = Sampling)
	EAlembicSamplingType SamplingType;

	/** Steps to take when sampling the animation*/
	UPROPERTY(EditAnywhere, Category = Sampling, meta = (EnumCondition = 1,ClampMin = "1"))
	uint32 FrameSteps;

	/** Time steps to take when sampling the animation*/
	UPROPERTY(EditAnywhere, Category = Sampling, meta = (EnumCondition = 2, ClampMin = "0.0001"))
	float TimeSteps;
	
	/** Starting index to start sampling the animation from*/
	UPROPERTY(EditAnywhere, Category = Sampling, meta = (ClampMin="0"))
	uint32 FrameStart;

	/** Ending index to stop sampling the animation at*/
	UPROPERTY(EditAnywhere, Category = Sampling)
	uint32 FrameEnd;

	/** Skip empty (pre-roll) frames and start importing at the frame which actually contains data */
	UPROPERTY(EditAnywhere, Category = Sampling, meta=(DisplayName = "Skip Empty Frames at Start of Alembic Sequence"))
	bool bSkipEmpty;
};

USTRUCT()
struct FAbcNormalGenerationSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcNormalGenerationSettings()
	{
		bRecomputeNormals = false;
		HardEdgeAngleThreshold = 0.9f;
		bForceOneSmoothingGroupPerObject = false;
		bIgnoreDegenerateTriangles = true;
	}

	/** Whether or not to force smooth normals for each individual object rather than calculating smoothing groups */
	UPROPERTY(EditAnywhere, Category = NormalCalculation)
	bool bForceOneSmoothingGroupPerObject;

	/** Threshold used to determine whether an angle between two normals should be considered hard, closer to 0 means more smooth vs 1 */
	UPROPERTY(EditAnywhere, Category = NormalCalculation, meta = (EditCondition = "!bForceOneSmoothingGroupPerObject", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HardEdgeAngleThreshold;

	/** Determines whether or not the normals should be forced to be recomputed */
	UPROPERTY(EditAnywhere, Category = NormalCalculation)
	bool bRecomputeNormals;

	/** Determines whether or not the degenerate triangles should be ignored when calculating tangents/normals */
	UPROPERTY(EditAnywhere, Category = NormalCalculation)
	bool bIgnoreDegenerateTriangles;
};

USTRUCT()
struct FAbcMaterialSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcMaterialSettings()
		: bCreateMaterials(false)
		, bFindMaterials(false)
	{}

	/** Whether or not to create materials according to found Face Set names (will not work without face sets) */
	UPROPERTY(EditAnywhere, Category = Materials)
	bool bCreateMaterials;

	/** Whether or not to try and find materials according to found Face Set names (will not work without face sets) */
	UPROPERTY(EditAnywhere, Category = Materials)
	bool bFindMaterials;
};


USTRUCT()
struct FAbcStaticMeshSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcStaticMeshSettings()
		: bMergeMeshes(true),
		bPropagateMatrixTransformations(true)
	{}
	
	// Whether or not to merge the static meshes on import (remember this can cause problems with overlapping UV-sets)
	UPROPERTY(EditAnywhere, Category = StaticMesh)
	bool bMergeMeshes;

	// This will, if applicable, apply matrix transformations to the meshes before merging
	UPROPERTY(EditAnywhere, Category = StaticMesh, meta=(editcondition="bMergeMeshes"))
	bool bPropagateMatrixTransformations;
};

/** Enum that describes type of asset to import */
UENUM()
enum class EAbcConversionPreset : uint8
{
	/** Imports only the first frame as one or multiple static meshes */
	Maya UMETA(DisplayName = "Autodesk Maya"),
	/** Imports the Alembic file as flipbook and matrix animated objects */
	Max UMETA(DisplayName = "Autodesk 3ds Max"),
	Custom UMETA(DisplayName = "Custom Settings")
};

USTRUCT()
struct FAbcConversionSettings
{
	GENERATED_USTRUCT_BODY()

	FAbcConversionSettings()
		: Preset(EAbcConversionPreset::Maya)
		, bFlipU(false)
		, bFlipV(true)
		, Scale(FVector(1.0f, -1.0f, 1.0f))
		, Rotation(FVector::ZeroVector)
	{}

	/** Currently preset that should be applied */
	UPROPERTY(EditAnywhere, Category = Conversion)
	EAbcConversionPreset Preset;

	/** Flag whether or not to flip the U channel in the Texture Coordinates */
	UPROPERTY(EditAnywhere, Category = Conversion)
	bool bFlipU;

	/** Flag whether or not to flip the V channel in the Texture Coordinates */
	UPROPERTY(EditAnywhere, Category = Conversion)
	bool bFlipV;

	/** Scale value that should be applied */
	UPROPERTY(EditAnywhere, Category = Conversion)
	FVector Scale;

	/** Rotation in Euler angles that should be applied */
	UPROPERTY(EditAnywhere, Category = Conversion)
	FVector Rotation;
};

/** Class that contains all options for importing an alembic file */
UCLASS()
class ALEMBICLIBRARY_API UAbcImportSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Accessor and initializer **/
	static UAbcImportSettings* Get();

	/** Type of asset to import from Alembic file */
	UPROPERTY(EditAnywhere, Category = Alembic)
	EAlembicImportType ImportType;
	
	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = Sampling)
	FAbcSamplingSettings SamplingSettings;

	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = NormalCalculation)
	FAbcNormalGenerationSettings NormalGenerationSettings;	
		
	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = Compression)
	FAbcCompressionSettings CompressionSettings;

	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = Materials)
	FAbcMaterialSettings MaterialSettings;

	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = StaticMesh)
	FAbcStaticMeshSettings StaticMeshSettings;

	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties), Category = Conversion)
	FAbcConversionSettings ConversionSettings;

	bool bReimport;
};
