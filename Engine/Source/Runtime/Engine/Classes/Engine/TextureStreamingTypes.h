// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Structs and defines used for texture streaming build

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "HAL/IConsoleManager.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "TextureStreamingTypes.generated.h"

class ULevel;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture2D;
struct FMaterialTextureInfo;
struct FMeshUVChannelInfo;
struct FSlowTask;
struct FStreamingTextureBuildInfo;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(TextureStreamingBuild, Log, All);

class UTexture;
class UTexture2D;
struct FStreamingTextureBuildInfo;
struct FMaterialTextureInfo;

// The PackedRelativeBox value that return the bound unaltered
static const uint32 PackedRelativeBox_Identity = 0xffff0000;

/** Information about a streaming texture that a primitive uses for rendering. */
USTRUCT()
struct FStreamingTexturePrimitiveInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UTexture2D* Texture;

	/** 
	 * The streaming bounds of the texture, usually the component material bounds. 
	 * Usually only valid for registered component, as component bounds are only updated when the components are registered.
	 * otherwise only PackedRelativeBox can be used.Irrelevant when the component is not registered, as the component could be moved by ULevel::ApplyWorldOffset()
	 * In that case, only PackedRelativeBox is meaningful.
	 */
	UPROPERTY()
	FBoxSphereBounds Bounds;

	UPROPERTY()
	float TexelFactor;

	/** 
	 * When non zero, this represents the relative box used to compute Bounds, using the component bounds as reference.
	 * If available, this allows the texture streamer to generate the level streaming data before the level gets visible.
	 * At that point, the component are not yet registered, and the bounds are unknown, but the precompiled build data is still available.
	 * Also allows to update the relative bounds after a level get moved around from ApplyWorldOffset.
	 */
	UPROPERTY()
	uint32 PackedRelativeBox;

	FStreamingTexturePrimitiveInfo() : 
		Texture(nullptr), 
		Bounds(ForceInit), 
		TexelFactor(1.0f),
		PackedRelativeBox(0)
	{
	}

	FStreamingTexturePrimitiveInfo(UTexture2D* InTexture, const FBoxSphereBounds& InBounds, float InTexelFactor, uint32 InPackedRelativeBox = 0) :
		Texture(InTexture), 
		Bounds(InBounds), 
		TexelFactor(InTexelFactor),
		PackedRelativeBox(InPackedRelativeBox)
	{
	}

};

/** 
 * This struct holds the result of TextureStreaming Build for each component texture, as referred by its used materials.
 * It is possible that the entry referred by this data is not actually relevant in a given quality / target.
 * It is also possible that some texture are not referred, and will then fall on fallbacks computation.
 * Because each component holds its precomputed data for each texture, this struct is designed to be as compact as possible.
 */
USTRUCT()
struct FStreamingTextureBuildInfo
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * The relative bounding box for this entry. The relative bounds is a bound equal or smaller than the component bounds and represent
	 * the merged LOD section bounds of all LOD section referencing the given texture. When the level transform is modified following
	 * a call to ApplyLevelTransform, this relative bound becomes deprecated as it was computed from the transform at build time.
	 */
	UPROPERTY()
	uint32 PackedRelativeBox;

	/** 
	 * The level scope identifier of the texture. When building the texture streaming data, each level holds a list of all referred texture Guids.
	 * This is required to prevent loading textures on platforms which would not require the texture to be loaded, and is a consequence of the texture
	 * streaming build not being platform specific (the same streaming data is build for every platform target). Could also apply to quality level.
	 */
	UPROPERTY()
	int32 TextureLevelIndex;

	/** 
	 * The texel factor for this texture. This represent the world size a texture square holding with unit UVs.
	 * This value is a combination of the TexelFactor from the mesh and also the material scale.
	 * It does not take into consideration StreamingDistanceMultiplier, or texture group scale.
	 */
	UPROPERTY()
	float TexelFactor;

	FStreamingTextureBuildInfo() : 
		PackedRelativeBox(0), 
		TextureLevelIndex(0), 
		TexelFactor(0) 
	{
	}

	/**
	 *	Set this struct to match the unpacked params.
	 *
	 *	@param	LevelTextures	[in,out]	The list of textures referred by all component of a level. The array index maps to UTexture2D::LevelIndex.
	 *	@param	RefBounds		[in]		The reference bounds used to compute the packed relative box.
	 *	@param	Info			[in]		The unpacked params.
	 */
	ENGINE_API void PackFrom(ULevel* Level, const FBoxSphereBounds& RefBounds, const FStreamingTexturePrimitiveInfo& Info);

};

// The max number of uv channels processed in the texture streaming build.
#define TEXSTREAM_MAX_NUM_UVCHANNELS  4
// The initial texture scales (must be bigger than actual used values)
#define TEXSTREAM_INITIAL_GPU_SCALE 256
// The tile size when outputting the material texture scales.
#define TEXSTREAM_TILE_RESOLUTION 32
// The max number of textures processed in the material texture scales build.
#define TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL 32

struct FPrimitiveMaterialInfo
{
	FPrimitiveMaterialInfo() : Material(nullptr), UVChannelData(nullptr), PackedRelativeBox(0) {}

	// The material
	const UMaterialInterface* Material;

	// The mesh UV channel data
	const FMeshUVChannelInfo* UVChannelData;

	// The material bounds for the mesh.
	uint32 PackedRelativeBox;

	bool IsValid() const { return Material && UVChannelData && PackedRelativeBox != 0; }
};

enum ETextureStreamingBuildType
{
	TSB_MapBuild,
	TSB_ValidationOnly,
	TSB_ViewMode
};

/** 
 * Context used to resolve FStreamingTextureBuildInfo to FStreamingTexturePrimitiveInfo
 * The context make sure that build data and each texture is only processed once per component (with constant time).
 * It manage internally structures used to accelerate the binding between precomputed data and textures,
 * so that there is only one map lookup per texture per level. 
 * There is some complexity here because the build data does not reference directly texture objects to avoid hard references
 * which would load texture when the component is loaded, which could be wrong since the build data is built for a specific
 * feature level and quality level. The current feature and quality could reference more or less textures.
 * This requires the logic to not submit a streaming entry for precomputed data, as well as submit fallback data for 
 * texture that were referenced in the texture streaming build.
 */
class FStreamingTextureLevelContext
{
	/** Reversed lookup for ULevel::StreamingTextureGuids. */
	const TMap<FGuid, int32>* TextureGuidToLevelIndex;

	/** Whether the precomputed relative bounds should be used or not.  Will be false if the transform level was rotated since the last texture streaming build. */
	bool bUseRelativeBoxes;

	/** An id used to identify the component build data. */
	int32 BuildDataTimestamp;

	/** The last bound component texture streaming build data. */
	const TArray<FStreamingTextureBuildInfo>* ComponentBuildData;

	struct FTextureBoundState
	{
		FTextureBoundState() {}

		FTextureBoundState(UTexture2D* InTexture) : BuildDataTimestamp(0), BuildDataIndex(0), Texture(InTexture) {}

		/** The timestamp of the build data to indentify whether BuildDataIndex is valid or not. */
		int32 BuildDataTimestamp;
		/** The ComponentBuildData Index referring this texture. */
		int32 BuildDataIndex;
		/**  The texture relative to this entry. */
		UTexture2D* Texture;
	};

	/*
	 * The component state of the each texture. Used to prevent processing each texture several time.
	 * Also used to find quickly the build data relating to each texture. 
	 */
	TArray<FTextureBoundState> BoundStates;

	EMaterialQualityLevel::Type QualityLevel;
	ERHIFeatureLevel::Type FeatureLevel;

	int32* GetBuildDataIndexRef(UTexture2D* Texture2D);

public:

	// Needs InLevel to use precomputed data from 
	FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, const ULevel* InLevel = nullptr, const TMap<FGuid, int32>* InTextureGuidToLevelIndex = nullptr);
	FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, ERHIFeatureLevel::Type InFeatureLevel, bool InUseRelativeBoxes);
	FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, const UPrimitiveComponent* Primitive);
	~FStreamingTextureLevelContext();

	void BindBuildData(const TArray<FStreamingTextureBuildInfo>* PreBuiltData);
	void ProcessMaterial(const FBoxSphereBounds& ComponentBounds, const FPrimitiveMaterialInfo& MaterialData, float ComponentScaling, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures);

	EMaterialQualityLevel::Type GetQualityLevel() { return QualityLevel; }
	ERHIFeatureLevel::Type GetFeatureLevel() { return FeatureLevel; }
};


/** A Map that gives the (smallest) texture coordinate scale used when sampling each texture register of a shader.
 * The array index is the register index, and the value, is the coordinate scale.
 * Since a texture resource can be bound to several texture registers, it can related to different indices.
 * This is reflected in UMaterialInterface::GetUsedTexturesAndIndices where each texture is bound to 
 * an array of texture register indices.
 */
typedef TMap<UMaterialInterface*, TArray<FMaterialTextureInfo> > FTexCoordScaleMap;

/** A mapping between used material and levels for refering primitives. */
typedef TMap<UMaterialInterface*, TArray<ULevel*> > FMaterialToLevelsMap;

/** Build the shaders required for the texture streaming build. Returns whether or not the action was successful. */
ENGINE_API bool BuildTextureStreamingComponentData(UWorld* InWorld, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bFullRebuild, FSlowTask& BuildTextureStreamingTask);

/** Check if the lighting build is dirty. Updates the needs rebuild status of the level and world. */
ENGINE_API void CheckTextureStreamingBuildValidity(UWorld* InWorld);

/**
 * Checks whether a UTexture2D is a texture with streamable mips
 * @param Texture	Texture to check
 * @return			true if the UTexture2D is supposed to be streaming
 */
ENGINE_API bool IsStreamingTexture( const UTexture2D* Texture2D );

ENGINE_API uint32 PackRelativeBox(const FVector& RefOrigin, const FVector& RefExtent, const FVector& Origin, const FVector& Extent);
ENGINE_API uint32 PackRelativeBox(const FBox& RefBox, const FBox& Box);
ENGINE_API void UnpackRelativeBox(const FBoxSphereBounds& InRefBounds, uint32 InPackedRelBox, FBoxSphereBounds& OutBounds);


extern ENGINE_API TAutoConsoleVariable<int32> CVarStreamingUseNewMetrics;