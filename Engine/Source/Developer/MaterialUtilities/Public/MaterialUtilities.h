// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "SceneTypes.h"
#include "Modules/ModuleInterface.h"
#include "Engine/TextureStreamingTypes.h"
#include "UObject/ErrorException.h"
#include "Engine/Texture.h"

#include "LightMap.h"
#include "ShadowMap.h"

class ALandscapeProxy;
class Error;
class FStaticLODModel;
class ULandscapeComponent;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UTexture2D;
class UTextureRenderTarget2D;
class UMaterialOptions;
struct FMaterialProxySettings;
struct FRawMesh;
struct FBakeOutput;
struct FMeshData;
struct FMaterialData;

/* TODO replace this with rendering property enum when extending the system */
UENUM()
enum class EFlattenMaterialProperties : uint8
{
	Diffuse,
	Metallic,
	Specular,
	Roughness,	
	Normal,
	Opacity,
	Emissive,
	SubSurface,
	OpacityMask,
	AmbientOcclusion = 16,
	NumFlattenMaterialProperties
};

/** Structure used for storing intermediate baked down material data/samples*/
struct FFlattenMaterial
{
	FFlattenMaterial()
		: RenderSize(0, 0)
		, bTwoSided(false)
		, bDitheredLODTransition(false)
		, BlendMode(BLEND_Opaque)
		, EmissiveScale(1.0f)
	{
		for (FIntPoint& Size : PropertySizes)
		{
			Size = FIntPoint(ForceInitToZero);
		}
	}

	/** Release all the sample data */
	void ReleaseData()
	{
		for (TArray<FColor>& Samples : PropertySamples)
		{
			Samples.Empty();
		}
	}
	
	/** Set all alpha channel values with InAlphaValue */
	void FillAlphaValues(const uint8 InAlphaValue)
	{
		for (TArray<FColor>& SampleArray : PropertySamples)
		{
			for (FColor& Sample : SampleArray)
			{
				Sample.A = InAlphaValue;
			}
		}
	}

	const bool DoesPropertyContainData(const EFlattenMaterialProperties Property) const { return PropertySamples[(int32)Property].Num() > 0; }

	const bool IsPropertyConstant(const EFlattenMaterialProperties Property) const { return PropertySamples[(int32)Property].Num() == 1; }

	const bool ShouldGenerateDataForProperty(const EFlattenMaterialProperties Property) const { return PropertySizes[(int32)Property].GetMin() > 0; }

	const FIntPoint GetPropertySize(const EFlattenMaterialProperties Property) const{ return PropertySizes[(int32)Property]; }
	void SetPropertySize(const EFlattenMaterialProperties Property, const FIntPoint& InSize) { PropertySizes[(int32)Property] = InSize; }

	TArray<FColor>& GetPropertySamples(const EFlattenMaterialProperties Property) { return PropertySamples[(int32)Property]; }
	const TArray<FColor>& GetPropertySamples(const EFlattenMaterialProperties Property) const { return PropertySamples[(int32)Property]; }
	
	/** Material Guid */
	FGuid			MaterialId;	
	FIntPoint		RenderSize;

	/** Flag whether or not the material will have to be two-sided */
	bool			bTwoSided;
	/** Flag whether or not the material will use dithered LOD transitions */
	bool			bDitheredLODTransition;
	/** Blend mode for the new material */
	EBlendMode		BlendMode;
	/** Scale (maximum baked down value) for the emissive property */
	float			EmissiveScale;
private:

	/** Texture sizes for each individual property*/
	FIntPoint PropertySizes[(uint32)EFlattenMaterialProperties::NumFlattenMaterialProperties];
	/** Baked down texture samples for each individual property*/
	TArray<FColor> PropertySamples[(uint32)EFlattenMaterialProperties::NumFlattenMaterialProperties];
};

/** Export material proxy cache*/
struct MATERIALUTILITIES_API FExportMaterialProxyCache
{
	// Material proxies for each property. Note: we're not handling all properties here,
	// so hold only up to MP_Normal inclusive.
	class FMaterialRenderProxy* Proxies[MP_MAX];

	FExportMaterialProxyCache();
	~FExportMaterialProxyCache();

	/** Releasing the cached render proxies */
	void Release();
};

/** Intermediate material merging data */
struct FMaterialMergeData
{
	/** Material proxy cache, eliminates shader compilations when a material is baked out multiple times for different meshes */
	FExportMaterialProxyCache* ProxyCache;

	/** Input data */
	/** Material that is being baked out */
	class UMaterialInterface* Material;
	/** Raw mesh data used to bake out the material with, optional */
	const struct FRawMesh* Mesh;
	/** LODModel data used to bake out the material with, optional */
	const class FStaticLODModel* LODModel;
	/** Material index to use when the material is baked out using mesh data (face material indices) */
	int32 MaterialIndex;
	/** Optional tex coordinate bounds of original texture coordinates set */
	FBox2D TexcoordBounds;
	/** Optional new set of non-overlapping texture coordinates */
	const TArray<FVector2D>& TexCoords;

	FLightMapRef LightMap;
	FShadowMapRef ShadowMap;
	FUniformBufferRHIRef Buffer;
	int32 LightMapIndex;

	/** Output emissive scale, maximum baked out emissive value (used to scale other samples, 1/EmissiveScale * Sample) */
	float EmissiveScale;

	FMaterialMergeData(
		UMaterialInterface* InMaterial,
		const FRawMesh* InMesh,
		const FStaticLODModel* InLODModel,
		int32 InMaterialIndex,
		FBox2D InTexcoordBounds,
		const TArray<FVector2D>& InTexCoords)
		: ProxyCache(nullptr)
		, Material(InMaterial)
		, Mesh(InMesh)
		, LODModel(InLODModel)
		, MaterialIndex(InMaterialIndex)
		, TexcoordBounds(InTexcoordBounds)
		, TexCoords(InTexCoords)
		, EmissiveScale(0.0f)
	{
		ProxyCache = new FExportMaterialProxyCache();
	}

	~FMaterialMergeData()
	{
		if (ProxyCache)
		{
			delete ProxyCache;
		}
	}
};


class UMaterialInterface;
class UMaterial;
class UTexture2D;
class UTextureRenderTarget2D;
class UWorld;
class ALandscapeProxy;
class ULandscapeComponent;
class FPrimitiveComponentId;
class UMaterialInstanceConstant;
struct FMaterialMergeData;

/**
 * Material utilities
 */
class MATERIALUTILITIES_API FMaterialUtilities : public IModuleInterface
{
public:
	/** Begin IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	/** End IModuleInterface implementation */

	/**
	 * Whether material utilities support exporting specified material blend mode and property 
	 */
	static bool SupportsExport(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty);

	/**
	 * Renders specified material property into texture
	 *
	 * @param InWorld				World object to use for material property rendering
	 * @param InMaterial			Target material
	 * @param InMaterialProperty	Material property to render
	 * @param InRenderTarget		Render target to render to
	 * @param OutBMP				Output array of rendered samples 
	 * @return						Whether operation was successful
	 */	
	DEPRECATED(4.11, "Please use ExportMaterialProperty function with new signature")
	static bool ExportMaterialProperty(UWorld* InWorld, UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP);

	/**
	* Renders specified material property into texture
	*
	* @param InMaterial			Target material
	* @param InMaterialProperty	Material property to render	
	* @param OutBMP				Output array of rendered samples
	* @param OutSize			Output size of the rendered samples
	* @return					Whether operation was successful
	*/
	static bool ExportMaterialProperty(UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, TArray<FColor>& OutBMP, FIntPoint& OutSize );

	/**
	* Renders specified material property into texture
	*
	* @param InMaterial			Target material
	* @param InMaterialProperty	Material property to render
	* @param InSize				Input size for the rendered samples
	* @param OutBMP				Output array of rendered samples	
	* @return					Whether operation was successful
	*/
	static bool ExportMaterialProperty(UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, FIntPoint InSize, TArray<FColor>& OutBMP );

	/**
	 * Renders specified material property into texture
	 *
	 * @param InWorld				World object to use for material property rendering
	 * @param InMaterial			Target material
	 * @param InMaterialProperty	Material property to render
	 * @param OutBMP				Output array of rendered samples 
	 * @return						Whether operation was successful
	 */
	DEPRECATED(4.11, "Please use ExportMaterialProperty function with new signature")
	static bool ExportMaterialProperty(UWorld* InWorld, UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, FIntPoint& OutSize, TArray<FColor>& OutBMP);

	/**
	 * Flattens specified material
	 *
	 * @param InWorld				World object to use for material rendering
	 * @param InMaterial			Target material
	 * @param OutFlattenMaterial	Output flattened material
	 * @return						Whether operation was successful
	 */
	DEPRECATED(4.11, "Please use ExportMaterial function with new signature")
	static bool ExportMaterial(UWorld* InWorld, UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial);
	
	/**
	* Flattens specified material
	*
	* @param InMaterial				Target material
	* @param OutFlattenMaterial		Output flattened material
	* @return						Whether operation was successful
	*/
	static bool ExportMaterial(UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache = nullptr);

	DEPRECATED(4.17, "Please use new functionality in MaterialBaking module")
	static bool ExportMaterials(TArray<FMaterialMergeData*>& MergeData, TArray<FFlattenMaterial*>& OutFlattenMaterials);

	/**
	* Flattens specified material using mesh data	
	*
	* @param InMaterial			Target material
	* @param InMesh				Mesh data to use for flattening
	* @param InMaterialIndex	Material index
	* @param InTexcoordBounds	Texture bounds 
	* @param InTexCoords		Replacement non-overlapping texture coordinates
	* @param OutFlattenMaterial Output flattened material
	* @return					Whether operation was successful
	*/
	static bool ExportMaterial(UMaterialInterface* InMaterial, const FRawMesh* InMesh, int32 InMaterialIndex, const FBox2D& InTexcoordBounds, const TArray<FVector2D>& InTexCoords, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache = nullptr);

	static bool ExportMaterial(UMaterialInterface* InMaterial, const FRawMesh* InMesh, int32 InMaterialIndex, const FBox2D& InTexcoordBounds, const TArray<FVector2D>& InTexCoords, const int32 LightMapIndex, FLightMapRef LightMap, FShadowMapRef ShadowMap, FUniformBufferRHIRef Buffer, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache = nullptr);

	/**
	 * Flattens specified landscape material
	 *
	 * @param InLandscape			Target landscape
	 * @param HiddenPrimitives		Primitives to hide while rendering scene to texture
	 * @param OutFlattenMaterial	Output flattened material
	 * @return						Whether operation was successful
	 */
	static bool ExportLandscapeMaterial(ALandscapeProxy* InLandscape, const TSet<FPrimitiveComponentId>& HiddenPrimitives, FFlattenMaterial& OutFlattenMaterial);
	
	/**
 	 * Generates a texture from an array of samples 
	 *
	 * @param Outer					Outer for the material and texture objects, if NULL new packages will be created for each asset
	 * @param AssetLongName			Long asset path for the new texture
	 * @param Size					Resolution of the texture to generate (must match the number of samples)
	 * @param Samples				Color data for the texture
	 * @param CompressionSettings	Compression settings for the new texture
	 * @param LODGroup				LODGroup for the new texture
	 * @param Flags					ObjectFlags for the new texture
	 * @param bSRGB					Whether to set the bSRGB flag on the new texture
	 * @param SourceGuidHash		(optional) Hash (stored as Guid) to use part of the texture source's DDC key.
	 * @return						The new texture.
	 */
	static UTexture2D* CreateTexture(UPackage* Outer, const FString& AssetLongName, FIntPoint Size, const TArray<FColor>& Samples, TextureCompressionSettings CompressionSettings, TextureGroup LODGroup, EObjectFlags Flags, bool bSRGB, const FGuid& SourceGuidHash = FGuid());

	/**
	 * Creates UMaterial object from a flatten material
	 *
	 * @param Outer					Outer for the material and texture objects, if NULL new packages will be created for each asset
	 * @param BaseName				BaseName for the material and texture objects, should be a long package name in case Outer is not specified
	 * @param Flags					Object flags for the material and texture objects.
	 * @param OutGeneratedAssets	List of generated assets - material, textures
	 * @return						Returns a pointer to the constructed UMaterial object.
	 */
	static UMaterial* CreateMaterial(const FFlattenMaterial& InFlattenMaterial, UPackage* InOuter, const FString& BaseName, EObjectFlags Flags, const struct FMaterialProxySettings& MaterialProxySettings, TArray<UObject*>& OutGeneratedAssets, const TextureGroup& InTextureGroup = TEXTUREGROUP_World);

	/** 
	* Creates an instanced material based of BaseMaterial
	* @param Outer					Outer for the material and texture objects, if NULL new packages will be created for each asset
	* @param BaseName				BaseName for the material and texture objects, should be a long package name in case Outer is not specified
	* @param Flags					Object flags for the material and texture objects.
	* @return						Returns a pointer to the constructed UMaterialInstanceConstant object.
	*/

	static UMaterialInstanceConstant* CreateInstancedMaterial(UMaterial* BaseMaterial, UPackage* InOuter, const FString& BaseName, EObjectFlags Flags);

	/**
	* Creates bakes textures for a ULandscapeComponent
	*
	* @param LandscapeComponent		The component to bake textures for
	* @return						Whether operation was successful
	*/
	static bool ExportBaseColor(ULandscapeComponent* LandscapeComponent, int32 TextureSize, TArray<FColor>& OutSamples);

	/**
	* Creates a FFlattenMaterial instance with the given MaterialProxySettings data
	*
	* @param InMaterialLODSettings  Settings containing how to material should be merged
	* @return						Set up FFlattenMaterial according to the given settings
	*/
	static FFlattenMaterial CreateFlattenMaterialWithSettings(const FMaterialProxySettings& InMaterialLODSettings);

	/**
	* Analyzes given material to determine how many texture coordinates and whether or not vertex colors are used within the material Graph
	*
	* @param InMaterial 			Material to analyze
	* @param InMaterialSettings 	Settings containing how to material should be merged
	* @param OutNumTexCoords		Number of texture coordinates used across all properties flagged for merging
	* @param OutRequiresVertexData	Flag whether or not Vertex Data is used in the material graph for the properties flagged for merging
	*/
	static void AnalyzeMaterial(class UMaterialInterface* InMaterial, const struct FMaterialProxySettings& InMaterialSettings, int32& OutNumTexCoords, bool& OutRequiresVertexData);

	static void AnalyzeMaterial(class UMaterialInterface* InMaterial, const TArray<EMaterialProperty>& Properties, int32& OutNumTexCoords, bool& OutRequiresVertexData);

	/**
	* Remaps material indices where possible to reduce the number of materials required for creating a proxy material
	*
	* @param InMaterials 					List of Material interfaces (non-unique)
	* @param InMeshData						Array of meshes who use the materials in InMaterials
	* @param InMaterialMap					Map of MeshIDAndLOD keys with a material index array as value mapping InMeshData to the InMaterials array
	* @param InMaterialProxySettings		Settings for creating the proxy material
	* @param bBakeVertexData 				Flag whether or not Vertex Data should be baked down
	* @param bMergeMaterials 				Flag whether or not materials with be merged for this mesh
	* @param OutMeshShouldBakeVertexData	Array with Flag for each mesh whether or not Vertex Data should be baked down or is required to
	* @param OutMaterialMap					Map of MeshIDAndLOD keys with a material index array as value mapping InMeshData to the OutMaterials array
	* @param OutMaterials					List of Material interfaces (unique)
	*/
	static void RemapUniqueMaterialIndices(const TArray<struct FSectionInfo>& InSections, const TArray<struct FRawMeshExt>& InMeshData, const TMap<FIntPoint, TArray<int32> >& InMaterialMap, const FMaterialProxySettings& InMaterialProxySettings, const bool bBakeVertexData, const bool bMergeMaterials, TArray<bool>& OutMeshShouldBakeVertexData, TMap<FIntPoint, TArray<int32> >& OutMaterialMap, TArray<struct FSectionInfo>& OutSections);

	/**
	* Tries to optimize the flatten material's data by picking out constant values for the various properties
	*
	* @param InFlattenMaterial				Flatten material to optimize
	*/
	static void OptimizeFlattenMaterial(FFlattenMaterial& InFlattenMaterial);

	/**
	* Resizes flatten material's data if applicable by comparing it with the original settings
	*
	* @param InFlattenMaterial				Flatten material to optimize
	*/
	static void ResizeFlattenMaterial(FFlattenMaterial& InFlattenMaterial, const struct FMeshProxySettings& InMeshProxySettings);

	/** Tries to optimize the sample array (will set to const value if all samples are equal) */
	static void OptimizeSampleArray(TArray<FColor>& InSamples, FIntPoint& InSampleSize);

	/**
	* Contains errors generated when exporting material texcoord scales. 
	* Used to prevent displaying duplicates, as instances using the same shaders get the same issues.
	*/
	class MATERIALUTILITIES_API FExportErrorManager
	{
	public:

		FExportErrorManager(ERHIFeatureLevel::Type InFeatureLevel) : FeatureLevel(InFeatureLevel) {}

		enum EErrorType
		{
			EET_IncohorentValues,
			EET_NoValues
		};

		/**
		* Register a new error.
		*
		* @param Material			The material having this error.
		* @param TextureName		The texture for which the scale could not be generated.
		* @param RegisterIndex		The register index bound to this texture.
		* @param ErrorType			The issue encountered.
		*/
		void Register(const UMaterialInterface* Material, FName TextureName, int32 RegisterIndex, EErrorType ErrorType);

		/**
		* Output all errors registered.
		*/
		void OutputToLog();

	private:

		struct FError
		{
			const FMaterial* Material;
			int32 RegisterIndex;
			EErrorType ErrorType;

			bool operator==(const FError& Rhs) const;
		};

		struct FInstance
		{
			const UMaterialInterface* Material;
			FName TextureName;
		};

		friend uint32 GetTypeHash(const FError& Error);

		ERHIFeatureLevel::Type FeatureLevel;
		TMap<FError, TArray<FInstance> > ErrorInstances;
	};

	/**
	* Get the material texcoord scales applied on each textures
	*
	* @param InMaterial			Target material
	* @param QualityLevel		Quality level used for the shader profiling.
	* @param FeatureLevel		Feature level used for the shader profiling.
	* @param OutErrors			Manager to log errors (removes duplicates and similar errors)	
	* @return					Whether operation was successful
	*/
	static bool ExportMaterialUVDensities(UMaterialInterface* InMaterial, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, FExportErrorManager& OutErrors);

	/** Calculates an importance value for the given set of materials according to the texture sampler usage */
	static void DetermineMaterialImportance(const TArray<UMaterialInterface*>& InMaterials, TArray<float>& OutImportance);
	/** Generates a set of texture boxes according to the given weights and final atlas texture size */
	static void GeneratedBinnedTextureSquares(const FVector2D DestinationSize, TArray<float>& InTexureWeights, TArray<FBox2D>& OutGeneratedBoxes);

	/** Creates a proxy material and the required texture assets */
	static UMaterialInterface* CreateProxyMaterialAndTextures(UPackage* OuterPackage, const FString& AssetName, const FBakeOutput& BakeOutput, const FMeshData& MeshData, const FMaterialData& MaterialData, UMaterialOptions* Options);

	/** Creates a proxy material and the required texture assets */
	static UMaterialInterface* CreateProxyMaterialAndTextures(const FString& PackageName, const FString& AssetName, const FBakeOutput& BakeOutput, const FMeshData& MeshData, const FMaterialData& MaterialData, UMaterialOptions* Options);
private:
	
	/**
	* Private export material function to which unique signatures are mapped for unified code path
	*
	* @param InMaterialData			Target material data 
	* @param OutFlattenMaterial		Output flattened material
	* @return						Whether operation was successful
	*/
	static bool ExportMaterial(struct FMaterialMergeData& InMaterialData, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache = nullptr);

	/**
	* Renders out the specified material property with the given material data to a texture
	*
	* @param InMaterialData			Target material data
	* @param InMaterialProperty		Target material property
	* @param bInForceLinearGamma	Whether or not to force linear gamma (used for Normal property)
	* @param InPixelFormat			Pixel format of the target texture
	* @param InTargetSize			Dimensions of the target texture
	* @param OutSampleSize			Dimensions of the rendered texture
	* @param OutSamples				Array of FColor samples containing the rendered out texture pixel data
	* @return						Whether operation was successful
	*/
	static bool RenderMaterialPropertyToTexture(struct FMaterialMergeData& InMaterialData, EMaterialProperty InMaterialProperty, bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint InTargetSize, FIntPoint& OutSampleSize, TArray<FColor>& OutSamples);

	/**
	* Creates and add or reuses a RenderTarget from the pool
	*
	* @param bInForceLinearGamma	Whether or not to force linear gamma
	* @param InPixelFormat			Pixel format of the render target
	* @param InTargetSize			Dimensions of the render target
	* @return						Created render target
	*/
	static UTextureRenderTarget2D* CreateRenderTarget(bool bInForceLinearGamma, bool bNormalMap, EPixelFormat InPixelFormat, FIntPoint& InTargetSize );
	
	/** Clears the pool with available render targets */
	static void ClearRenderTargetPool();	

	/** Call back for garbage collector, cleans up the RenderTargetPool if CurrentlyRendering is set to false */
	void OnPreGarbageCollect();
private:
	/** Flag to indicate whether or not a texture is currently being rendered out */
	static bool CurrentlyRendering;
	/** Pool of available render targets, cached for re-using on consecutive property rendering */
	static TArray<UTextureRenderTarget2D*> RenderTargetPool;
};
