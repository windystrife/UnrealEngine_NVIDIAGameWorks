// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/MaterialMerging.h"
#include "MeshMerging.generated.h"

/** The importance of a mesh feature when automatically generating mesh LODs. */
UENUM()
namespace EMeshFeatureImportance
{
	enum Type
	{
		Off,
		Lowest,
		Low,
		Normal,
		High,
		Highest
	};
}

/** Settings used to reduce a mesh. */
USTRUCT()
struct FMeshReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Percentage of triangles to keep. 1.0 = no reduction, 0.0 = no triangles. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	float PercentTriangles;

	/** The maximum distance in object space by which the reduced mesh may deviate from the original mesh. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	float MaxDeviation;

	/** The amount of error in pixels allowed for this LOD. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	float PixelError;

	/** Threshold in object space at which vertices are welded together. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	float WeldingThreshold;

	/** Angle at which a hard edge is introduced between faces. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	float HardAngleThreshold;

	/** Higher values minimize change to border edges. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> SilhouetteImportance;

	/** Higher values reduce texture stretching. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> TextureImportance;

	/** Higher values try to preserve normals better. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> ShadingImportance;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	bool bRecalculateNormals;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	int32 BaseLODModel;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	bool bGenerateUniqueLightmapUVs;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	bool bKeepSymmetry;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	bool bVisibilityAided;

	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	bool bCullOccluded;

	/** Higher values generates fewer samples*/
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VisibilityAggressiveness;

	/** Higher values minimize change to vertex color data. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VertexColorImportance;

	/** Default settings. */
	FMeshReductionSettings()
		: PercentTriangles(1.0f)
		, MaxDeviation(0.0f)
		, PixelError(8.0f)
		, WeldingThreshold(0.0f)
		, HardAngleThreshold(80.0f)
		, SilhouetteImportance(EMeshFeatureImportance::Normal)
		, TextureImportance(EMeshFeatureImportance::Normal)
		, ShadingImportance(EMeshFeatureImportance::Normal)
		, bRecalculateNormals(false)
		, BaseLODModel(0)
		, bGenerateUniqueLightmapUVs(false)
		, bKeepSymmetry(false)
		, bVisibilityAided(false)
		, bCullOccluded(false)
		, VisibilityAggressiveness(EMeshFeatureImportance::Lowest)
		, VertexColorImportance(EMeshFeatureImportance::Off)
	{
	}

	FMeshReductionSettings(const FMeshReductionSettings& Other)
		: PercentTriangles(Other.PercentTriangles)
		, MaxDeviation(Other.MaxDeviation)
		, PixelError(Other.PixelError)
		, WeldingThreshold(Other.WeldingThreshold)
		, HardAngleThreshold(Other.HardAngleThreshold)
		, SilhouetteImportance(Other.SilhouetteImportance)
		, TextureImportance(Other.TextureImportance)
		, ShadingImportance(Other.ShadingImportance)
		, bRecalculateNormals(Other.bRecalculateNormals)
		, BaseLODModel(Other.BaseLODModel)
		, bGenerateUniqueLightmapUVs(Other.bGenerateUniqueLightmapUVs)
		, bKeepSymmetry(Other.bKeepSymmetry)
		, bVisibilityAided(Other.bVisibilityAided)
		, bCullOccluded(Other.bCullOccluded)
		, VisibilityAggressiveness(Other.VisibilityAggressiveness)
		, VertexColorImportance(Other.VertexColorImportance)
	{
	}

	/** Equality operator. */
	bool operator==(const FMeshReductionSettings& Other) const
	{
		return PercentTriangles == Other.PercentTriangles
			&& MaxDeviation == Other.MaxDeviation
			&& PixelError == Other.PixelError
			&& WeldingThreshold == Other.WeldingThreshold
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& SilhouetteImportance == Other.SilhouetteImportance
			&& TextureImportance == Other.TextureImportance
			&& ShadingImportance == Other.ShadingImportance
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& BaseLODModel == Other.BaseLODModel
			&& bGenerateUniqueLightmapUVs == Other.bGenerateUniqueLightmapUVs
			&& bKeepSymmetry == Other.bKeepSymmetry
			&& bVisibilityAided == Other.bVisibilityAided
			&& bCullOccluded == Other.bCullOccluded
			&& VisibilityAggressiveness == Other.VisibilityAggressiveness
			&& VertexColorImportance == Other.VertexColorImportance;
	}

	/** Inequality. */
	bool operator!=(const FMeshReductionSettings& Other) const
	{
		return !(*this == Other);
	}
};

UENUM()
namespace ELandscapeCullingPrecision
{
	enum Type
	{
		High = 0 UMETA(DisplayName = "High memory intensity and computation time"),
		Medium = 1 UMETA(DisplayName = "Medium memory intensity and computation time"),
		Low = 2 UMETA(DisplayName = "Low memory intensity and computation time")
	};
}

USTRUCT()
struct FMeshProxySettings
{
	GENERATED_USTRUCT_BODY()
	/** Screen size of the resulting proxy mesh in pixel size*/
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	int32 ScreenSize;

	/** Material simplification */
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	FMaterialProxySettings MaterialSettings;

	UPROPERTY()
	int32 TextureWidth_DEPRECATED;
	UPROPERTY()
	int32 TextureHeight_DEPRECATED;

	UPROPERTY()
	bool bExportNormalMap_DEPRECATED;

	UPROPERTY()
	bool bExportMetallicMap_DEPRECATED;

	UPROPERTY()
	bool bExportRoughnessMap_DEPRECATED;

	UPROPERTY()
	bool bExportSpecularMap_DEPRECATED;

	/** Determines whether or not the correct LOD models should be calculated given the source meshes and transition size */
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	bool bCalculateCorrectLODModel;

	/** Distance at which meshes should be merged together */
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	float MergeDistance;

	/** Angle at which a hard edge is introduced between faces */
	UPROPERTY(EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Hard Edge Angle"))
	float HardAngleThreshold;

	/** Lightmap resolution */
	UPROPERTY(EditAnywhere, Category = ProxySettings, meta = (EditCondition = "!bComputeLightMapResolution"))
	int32 LightMapResolution;

	/** If ticked will compute the lightmap resolution by summing the dimensions for each mesh included for merging */
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	bool bComputeLightMapResolution;

	/** Whether Simplygon should recalculate normals, otherwise the normals channel will be sampled from the original mesh */
	UPROPERTY(EditAnywhere, Category = ProxySettings)
	bool bRecalculateNormals;

	UPROPERTY()
	bool bBakeVertexData_DEPRECATED;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(EditAnywhere, Category = LandscapeCulling)
	bool bUseLandscapeCulling;

	/** Level of detail of the landscape that should be used for the culling */
	UPROPERTY(EditAnywhere, Category = LandscapeCulling, meta = (EditCondition="bUseLandscapeCulling"))
	TEnumAsByte<ELandscapeCullingPrecision::Type> LandscapeCullingPrecision;

	/** Default settings. */
	FMeshProxySettings()
		: ScreenSize(300)
		, TextureWidth_DEPRECATED(512)
		, TextureHeight_DEPRECATED(512)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, MergeDistance(4)
		, HardAngleThreshold(80.0f)
		, LightMapResolution(256)
		, bRecalculateNormals(true)
		, bUseLandscapeCulling(false)
	{ 
		MaterialSettings.MaterialMergeType = EMaterialMergeType::MaterialMergeType_Simplygon;
	}

	/** Equality operator. */
	bool operator==(const FMeshProxySettings& Other) const
	{
		return ScreenSize == Other.ScreenSize
			&& MaterialSettings == Other.MaterialSettings
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& MergeDistance == Other.MergeDistance;
	}

	/** Inequality. */
	bool operator!=(const FMeshProxySettings& Other) const
	{
		return !(*this == Other);
	}

	/** Handles deprecated properties */
	void PostLoadDeprecated();
};


UENUM()
enum class EMeshLODSelectionType : uint8
{
	// Whether or not to export all of the LODs found in the source meshes
	AllLODs = 0 UMETA(DisplayName = "Use all LOD levels"),	
	// Whether or not to export all of the LODs found in the source meshes
	SpecificLOD = 1 UMETA(DisplayName = "Use specific LOD level"),
	// Whether or not to calculate the appropriate LOD model for the given screen size
	CalculateLOD = 2 UMETA(DisplayName = "Calculate correct LOD level")
};

UENUM()
enum class EMeshMergeType : uint8
{
	MeshMergeType_Default,
	MeshMergeType_MergeActor
};

/**
* Mesh merging settings
*/
USTRUCT(Blueprintable)
struct FMeshMergingSettings
{
	GENERATED_USTRUCT_BODY()

	/** Whether to generate lightmap UVs for a merged mesh*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	bool bGenerateLightMapUV;

	/** Target lightmap resolution */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(ClampMax = 4096, editcondition = "!bComputedLightMapResolution"))
	int32 TargetLightMapResolution;

	/** Whether or not the lightmap resolution should be computed by summing the lightmap resolutions for the input Mesh Components */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings)
	bool bComputedLightMapResolution;

	/** Whether we should import vertex colors into merged mesh */
	UPROPERTY()
	bool bImportVertexColors_DEPRECATED;

	/** Whether merged mesh should have pivot at world origin, or at first merged component otherwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bPivotPointAtZero;

	/** Whether to merge physics data (collision primitives)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bMergePhysicsData;

	/** Whether to merge source materials into one flat material, ONLY available when merging a single LOD level, see LODSelectionType */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings)
	bool bMergeMaterials;

	/** Material simplification */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (editcondition = "bMergeMaterials"))
	FMaterialProxySettings MaterialSettings;

	/** Whether or not vertex data such as vertex colours should be baked into the resulting mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	bool bBakeVertexDataToMesh;

	/** Whether or not vertex data such as vertex colours should be used when baking out materials */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (editcondition = "bMergeMaterials"))
	bool bUseVertexDataForBakingMaterial;

	// Whether or not to calculate varying output texture sizes according to their importance in the final atlas texture
	UPROPERTY(Category = MaterialSettings, EditAnywhere, BlueprintReadWrite, meta = (editcondition = "bMergeMaterials"))
	bool bUseTextureBinning;
			
	UPROPERTY()
	bool bCalculateCorrectLODModel_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	EMeshLODSelectionType LODSelectionType;

	UPROPERTY()
	int32 ExportSpecificLOD_DEPRECATED;

	// A given LOD level to export from the source meshes
	UPROPERTY(EditAnywhere, Category = MeshSettings, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", EnumCondition = 1))
	int32 SpecificLOD;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LandscapeCulling)
	bool bUseLandscapeCulling;

	/** Whether to export normal maps for material merging */
	UPROPERTY()
	bool bExportNormalMap_DEPRECATED;
	/** Whether to export metallic maps for material merging */
	UPROPERTY()
	bool bExportMetallicMap_DEPRECATED;
	/** Whether to export roughness maps for material merging */
	UPROPERTY()
	bool bExportRoughnessMap_DEPRECATED;
	/** Whether to export specular maps for material merging */
	UPROPERTY()
	bool bExportSpecularMap_DEPRECATED;
	/** Merged material texture atlas resolution */
	UPROPERTY()
	int32 MergedMaterialAtlasResolution_DEPRECATED;

	EMeshMergeType MergeType;

	/** Default settings. */
	FMeshMergingSettings()
		: bGenerateLightMapUV(true)
		, TargetLightMapResolution(256)
		, bImportVertexColors_DEPRECATED(false)
		, bPivotPointAtZero(false)
		, bMergePhysicsData(false)
		, bMergeMaterials(false)
		, bBakeVertexDataToMesh(false)
		, bUseVertexDataForBakingMaterial(true)
		, bUseTextureBinning(false)
		, bCalculateCorrectLODModel_DEPRECATED(false)
		, LODSelectionType(EMeshLODSelectionType::CalculateLOD)
		, ExportSpecificLOD_DEPRECATED(0)
		, SpecificLOD(0)
		, bUseLandscapeCulling(false)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, MergedMaterialAtlasResolution_DEPRECATED(1024)
		, MergeType(EMeshMergeType::MeshMergeType_Default)
	{
	}

	/** Handles deprecated properties */
	void PostLoadDeprecated();
};

/** Struct to store per section info used to populate data after (multiple) meshes are merged together */
struct FSectionInfo
{
	/** Material used by the section */
	class UMaterialInterface* Material;
	/** Name value for the section */
	FName MaterialSlotName;	
	/** List of properties enabled for the section (collision, cast shadow etc) */
	TArray<FName> EnabledProperties;
	/** Original index of Material in the source data */
	int32 MaterialIndex;
	/** Index pointing to the start set of mesh indices that belong to this section */
	int32 StartIndex;
	/** Index pointing to the end set of mesh indices that belong to this section */
	int32 EndIndex;
	/** Used while baking out materials, to check which sections are and aren't being baked out */
	bool bProcessed;

	bool operator==(const FSectionInfo& Other) const
	{
		return Material == Other.Material && MaterialSlotName == Other.MaterialSlotName && EnabledProperties == Other.EnabledProperties && bProcessed == Other.bProcessed && MaterialIndex == Other.MaterialIndex && StartIndex == Other.StartIndex && EndIndex == Other.EndIndex;
	}
};
