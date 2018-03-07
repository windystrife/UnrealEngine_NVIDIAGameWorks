// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "RenderCommandFence.h"
#include "Templates/ScopedPointer.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "UniquePtr.h"

#include "Material.generated.h"

class ITargetPlatform;
class UMaterialExpressionComment;
class UPhysicalMaterial;
class USubsurfaceProfile;
class UTexture;

#if WITH_EDITOR

/** struct used for annotations when a materials 'used with' flags have changed and need saving */
struct FMaterialsWithDirtyUsageFlags
{
	/** store the flags that have been changed since last save, each bit represents a flag */
	uint32 MaterialFlagsThatHaveChanged;

	FMaterialsWithDirtyUsageFlags():
	MaterialFlagsThatHaveChanged(0)
	{
	}

	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default annotation
	 */
	FORCEINLINE bool IsDefault()
	{
		return MaterialFlagsThatHaveChanged == DefaultAnnotation.MaterialFlagsThatHaveChanged;
	}

	/** Mark the specificed flag as changed in this annotation */
	void MarkUsageFlagDirty(EMaterialUsage UsageFlag);

	/** Query the annotation to see if the specified flag has been changed */	
	bool IsUsageFlagDirty(EMaterialUsage UsageFlag);

	/** Default state for annotations (no flags changed)*/
	static const FMaterialsWithDirtyUsageFlags DefaultAnnotation;
};
#endif

/** Defines how the GBuffer channels are getting manipulated by a decal material pass. Actual index is used to control shader parameters so don't change order. */
UENUM()
enum EDecalBlendMode
{
	/** Blend full material, updating the GBuffer, does not work for baked lighting. */
	DBM_Translucent UMETA(DisplayName="Translucent"),
	/** Modulate BaseColor, blend rest, updating the GBuffer, does not work for baked lighting. */
	DBM_Stain UMETA(DisplayName="Stain"),
	/** Only blend normal, updating the GBuffer, does not work for baked lighting. */
	DBM_Normal UMETA(DisplayName="Normal"),
	/** Additive emissive only. */
	DBM_Emissive UMETA(DisplayName="Emissive"),

	/** Non metal, put into DBuffer to work for baked lighting as well (becomes DBM_TranslucentNormal if normal is not hooked up). */
	DBM_DBuffer_ColorNormalRoughness UMETA(DisplayName="DBuffer Translucent Color,Normal,Roughness"),
	/** Non metal, put into DBuffer to work for baked lighting as well. */
	DBM_DBuffer_Color UMETA(DisplayName="DBuffer Translucent Color"),
	/** Non metal, put into DBuffer to work for baked lighting as well (becomes DBM_DBuffer_Color if normal is not hooked up). */
	DBM_DBuffer_ColorNormal UMETA(DisplayName="DBuffer Translucent Color,Normal"),
	/** Non metal, put into DBuffer to work for baked lighting as well. */
	DBM_DBuffer_ColorRoughness UMETA(DisplayName="DBuffer Translucent Color,Roughness"),
	/** Non metal, put into DBuffer to work for baked lighting as well. */
	DBM_DBuffer_Normal UMETA(DisplayName="DBuffer Translucent Normal"),
	/** Non metal, put into DBuffer to work for baked lighting as well (becomes DBM_DBuffer_Roughness if normal is not hooked up). */
	DBM_DBuffer_NormalRoughness UMETA(DisplayName="DBuffer Translucent Normal,Roughness"),
	/** Non metal, put into DBuffer to work for baked lighting as well. */
	DBM_DBuffer_Roughness UMETA(DisplayName="DBuffer Translucent Roughness"),

	/** Output signed distance in Opacity depending on LightVector. Note: Can be costly, no shadow casting but receiving, no per pixel normal yet, no quality settings yet */
	DBM_Volumetric_DistanceFunction UMETA(DisplayName="Volumetric Distance Function (experimental)"),

	DBM_MAX,
};

inline bool IsDBufferDecalBlendMode(EDecalBlendMode In)
{
	switch(In)
	{
		case DBM_DBuffer_ColorNormalRoughness:
		case DBM_DBuffer_Color:
		case DBM_DBuffer_ColorNormal:
		case DBM_DBuffer_ColorRoughness:
		case DBM_DBuffer_Normal:
		case DBM_DBuffer_NormalRoughness:
		case DBM_DBuffer_Roughness:
			return true;
	}

	return false;
}

/** Defines how the material reacts on DBuffer decals, later we can expose more variants between None and Default. */
UENUM()
enum EMaterialDecalResponse
{
	/** Do not receive decals (Later we still can read the DBuffer channels to customize the effect, this frees up some interpolators). */
	MDR_None UMETA(DisplayName="None"),

	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_ColorNormalRoughness UMETA(DisplayName="Color Normal Roughness"),
	/** Receive Decals, applies color DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_Color UMETA(DisplayName="Color"),
	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_ColorNormal UMETA(DisplayName="Color Normal"),
	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_ColorRoughness UMETA(DisplayName="Color Roughness"),
	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_Normal UMETA(DisplayName="Normal"),
	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_NormalRoughness UMETA(DisplayName="Normal Roughness"),
	/** Receive Decals, applies all DBuffer channels, assumes the decal is non metal and mask the subsurface scattering. */
	MDR_Roughness UMETA(DisplayName="Roughness"),
	MDR_MAX
};

// NVCHANGE_BEGIN: Add VXGI
UENUM()
enum EVxgiMaterialSamplingRate
{
	VXGIMSR_FixedDefault UMETA(DisplayName = "Default (1X)"),
	VXGIMSR_Fixed2X UMETA(DisplayName = "Fixed 2X"),
	VXGIMSR_Fixed3X UMETA(DisplayName = "Fixed 3X"),
	VXGIMSR_Fixed4X UMETA(DisplayName = "Fixed 4X"),
	VXGIMSR_AdaptiveDefault UMETA(DisplayName = "Adaptive"),
	VXGIMSR_AdaptiveGE2 UMETA(DisplayName = "Adaptive >= 2X"),
	VXGIMSR_AdaptiveGE4 UMETA(DisplayName = "Adaptive >= 4X"),
	VXGIMSR_MAX,
};
// NVCHANGE_END: Add VXGI

// Material input structs.
//@warning: manually mirrored in MaterialShared.h
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FMaterialInput
{
#if WITH_EDITORONLY_DATA
	/** Material expression that this input is connected to, or NULL if not connected. */
	UPROPERTY()
	class UMaterialExpression* Expression;
#endif

	/** Index into Expression's outputs array that this input is connected to. */
	UPROPERTY()
	int32 OutputIndex;

	/** 
	 * Optional name of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	UPROPERTY()
	FString InputName;

	UPROPERTY()
	int32 Mask;

	UPROPERTY()
	int32 MaskR;

	UPROPERTY()
	int32 MaskG;

	UPROPERTY()
	int32 MaskB;

	UPROPERTY()
	int32 MaskA;

	/** Material expression name that this input is connected to, or None if not connected. Used only in cooked builds */
	UPROPERTY()
	FName ExpressionName;

};
#endif

#if !CPP      //noexport struct
USTRUCT(noexport)
struct FColorMaterialInput : public FMaterialInput
{
	UPROPERTY()
	uint32 UseConstant:1;

	UPROPERTY()
	FColor Constant;

};
#endif

#if !CPP      //noexport struct
USTRUCT(noexport)
struct FScalarMaterialInput : public FMaterialInput
{
	UPROPERTY()
	uint32 UseConstant:1;

	UPROPERTY()
	float Constant;

};
#endif

#if !CPP      //noexport struct
USTRUCT(noexport)
struct FVectorMaterialInput : public FMaterialInput
{
	UPROPERTY()
	uint32 UseConstant:1;

	UPROPERTY()
	FVector Constant;

};
#endif

#if !CPP      //noexport struct
USTRUCT(noexport)
struct FVector2MaterialInput : public FMaterialInput
{
	UPROPERTY()
	uint32 UseConstant:1;

	UPROPERTY()
	float ConstantX;

	UPROPERTY()
	float ConstantY;

};
#endif


/** Stores information about a function that this material references, used to know when the material needs to be recompiled. */
USTRUCT()
struct FMaterialFunctionInfo
{
	GENERATED_USTRUCT_BODY()

	/** Id that the function had when this material was last compiled. */
	UPROPERTY()
	FGuid StateId;

	/** The function which this material has a dependency on. */
	UPROPERTY()
	class UMaterialFunction* Function;

	FMaterialFunctionInfo()
		: Function(NULL)
	{}
};

/** Stores information about a parameter collection that this material references, used to know when the material needs to be recompiled. */
USTRUCT()
struct FMaterialParameterCollectionInfo
{
	GENERATED_USTRUCT_BODY()

	/** Id that the collection had when this material was last compiled. */
	UPROPERTY()
	FGuid StateId;

	/** The collection which this material has a dependency on. */
	UPROPERTY()
	class UMaterialParameterCollection* ParameterCollection;

	FMaterialParameterCollectionInfo()
		: ParameterCollection(NULL)
	{}

	bool operator==(const FMaterialParameterCollectionInfo& Other) const
	{
		return StateId == Other.StateId && ParameterCollection == Other.ParameterCollection;
	}
};

USTRUCT()
struct FParameterGroupData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Group Sorting")
	FString GroupName;

	UPROPERTY(EditAnywhere, Category = "Group Sorting")
	int32 GroupSortPriority;

	FParameterGroupData()
	{
		GroupName = FString(TEXT(""));
		GroupSortPriority = 0;
	}
};

/**
 * A Material is an asset which can be applied to a mesh to control the visual look of the scene. 
 * When light from the scene hits the surface, the shading model of the material is used to calculate how that light interacts with the surface. 
 *
 * Warning: Creating new materials directly increases shader compile times!  Consider creating a Material Instance off of an existing material instead.
 */
UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class UMaterial : public UMaterialInterface
{
	GENERATED_UCLASS_BODY()

	// Physics.
	
	/** Physical material to use for this graphics material. Used for sounds, effects etc.*/
	UPROPERTY(EditAnywhere, Category=PhysicalMaterial)
	class UPhysicalMaterial* PhysMaterial;

	// Reflection.
	
	UPROPERTY()
	FColorMaterialInput DiffuseColor_DEPRECATED;

	UPROPERTY()
	FColorMaterialInput SpecularColor_DEPRECATED;

	UPROPERTY()
	FColorMaterialInput BaseColor;

	UPROPERTY()
	FScalarMaterialInput Metallic;

	UPROPERTY()
	FScalarMaterialInput Specular;

	UPROPERTY()
	FScalarMaterialInput Roughness;

	UPROPERTY()
	FVectorMaterialInput Normal;

	// Emission.
	UPROPERTY()
	FColorMaterialInput EmissiveColor;

	// Transmission.
	UPROPERTY()
	FScalarMaterialInput Opacity;

	UPROPERTY()
	FScalarMaterialInput OpacityMask;

	/** 
	 * The domain that the material's attributes will be evaluated in. 
	 * Certain pieces of material functionality are only valid in certain domains, for example vertex normal is only valid on a surface.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Material, AssetRegistrySearchable)
	TEnumAsByte<enum EMaterialDomain> MaterialDomain;

	/** Determines how the material's color is blended with background colors. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category=Material, AssetRegistrySearchable)
	TEnumAsByte<enum EBlendMode> BlendMode;

	/** Defines how the GBuffer chanels are getting manipulated by a decal material pass. (only with MaterialDomain == MD_DeferredDecal) */
	UPROPERTY(EditAnywhere, Category=Material)
	TEnumAsByte<enum EDecalBlendMode> DecalBlendMode;

	/** 
	 * Defines how the material reacts on DBuffer decals (Affects look, performance and texture/sample usage).
	 * Non DBuffer Decals can be disabled on the primitive (e.g. static mesh)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Material, AdvancedDisplay, meta=(DisplayName = "Decal Response (DBuffer)"), AssetRegistrySearchable)
	TEnumAsByte<enum EMaterialDecalResponse> MaterialDecalResponse;

private:
	/** Determines how inputs are combined to create the material's final color. */
	UPROPERTY(EditAnywhere, Category=Material, AssetRegistrySearchable)
	TEnumAsByte<enum EMaterialShadingModel> ShadingModel;
public:

	/** If BlendMode is BLEND_Masked, the surface is not rendered where OpacityMask < OpacityMaskClipValue. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	float OpacityMaskClipValue;

	/**
	* If true, translucent materials will cast dynamic shadows according to their opacity.
	* OpacityMaskClipValue is used as the threshold value.
	*/
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 bCastDynamicShadowAsMasked:1;

	/** Adds to world position in the vertex shader. */
	UPROPERTY()
	FVectorMaterialInput WorldPositionOffset;

	/** Offset in world space applied to tessellated vertices. */
	UPROPERTY()
	FVectorMaterialInput WorldDisplacement;

	/** Multiplies the tessellation factors applied when a tessellation mode is set. */
	UPROPERTY()
	FScalarMaterialInput TessellationMultiplier;

	/** Inner material color, only used for ShadingModel=Subsurface */
	UPROPERTY()
	FColorMaterialInput SubsurfaceColor;

	/**  */
	UPROPERTY()
	FScalarMaterialInput ClearCoat;

	/**  */
	UPROPERTY()
	FScalarMaterialInput ClearCoatRoughness;

	/** output ambient occlusion to the GBuffer */
	UPROPERTY()
	FScalarMaterialInput AmbientOcclusion;

	/**
	 * output refraction index for translucent rendering
	 * Air:1.0 Water:1.333 Ice:1.3 Glass:~1.6 Diamond:2.42
	 */
	UPROPERTY()
	FScalarMaterialInput Refraction;

	/** 
	 * These inputs are evaluated in the vertex shader and allow artists to do arbitrary vertex shader operations and access them in the pixel shader.
	 * When unconnected or hidden they default to passing through the vertex UVs.
	 */
	UPROPERTY()
	FVector2MaterialInput CustomizedUVs[8];

	UPROPERTY()
	FMaterialAttributesInput MaterialAttributes;

	UPROPERTY()
	FScalarMaterialInput PixelDepthOffset;

	/** Indicates that the material should be rendered in the SeparateTranslucency Pass (not affected by DOF, requires bAllowSeparateTranslucency to be set in .ini). */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Render After DOF"), AdvancedDisplay)
	uint32 bEnableSeparateTranslucency:1;

	/** Indicates that the translucent material should not be affected by bloom or DOF. (Note: Depth testing is not available) */
	UPROPERTY(EditAnywhere, Category = Translucency, meta = (DisplayName = "Mobile Separate Translucency"), AdvancedDisplay)
	uint32 bEnableMobileSeparateTranslucency : 1;

	/**
	 * Indicates that the material should be rendered using responsive anti-aliasing. Improves sharpness of small moving particles such as sparks.
	 * Only use for small moving features because it will cause aliasing of the background.
	 */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Responsive AA"), AdvancedDisplay)
	uint32 bEnableResponsiveAA:1;

	/** SSR on translucency */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Screen Space Reflections"))
	uint32 bScreenSpaceReflections:1;

	/** Indicates that the material should be rendered without backface culling and the normal should be flipped for backfaces. */
	UPROPERTY(EditAnywhere, Category=Material)
	uint32 TwoSided:1;

	/** Whether meshes rendered with the material should support dithered LOD transitions. */
	UPROPERTY(EditAnywhere, Category = Material, AdvancedDisplay, meta = (DisplayName = "Dithered LOD Transition"))
	uint32 DitheredLODTransition : 1;

	/** Dither opacity mask. When combined with Temporal AA this can be used as a form of limited translucency which supports all lighting features. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 DitherOpacityMask:1;

	/** Whether the material should allow outputting negative emissive color values.  Only allowed on unlit materials. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 bAllowNegativeEmissiveColor:1;

	/** Number of customized UV inputs to display.  Unconnected customized UV inputs will just pass through the vertex UVs. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay, meta=(ClampMin=0))
	int32 NumCustomizedUVs;

	/** Sets the lighting mode that will be used on this material if it is translucent. */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Lighting Mode"))
	TEnumAsByte<enum ETranslucencyLightingMode> TranslucencyLightingMode;

	/** 
	 * Useful for artificially increasing the influence of the normal on the lighting result for translucency.
	 * A value larger than 1 increases the influence of the normal, a value smaller than 1 makes the lighting more ambient.
	 */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Directional Lighting Intensity"))
	float TranslucencyDirectionalLightingIntensity;

	/** Allows a translucenct material to be used with custom depth writing by compiling additional shaders. */
	UPROPERTY(EditAnywhere, Category=Translucency, AdvancedDisplay, meta=(DisplayName = "Allow Custom Depth Writes"))
	uint32 AllowTranslucentCustomDepthWrites:1;

	/** Scale used to make translucent shadows more or less opaque than the material's actual opacity. */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Shadow Density Scale"))
	float TranslucentShadowDensityScale;

	/** 
	 * Scale used to make translucent self-shadowing more or less opaque than the material's shadow on other objects. 
	 * This is only used when the object is casting a volumetric translucent shadow.
	 */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Self Shadow Density Scale"))
	float TranslucentSelfShadowDensityScale;

	/** Used to make a second self shadow gradient, to add interesting shading in the shadow of the first. */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Second Density Scale"))
	float TranslucentSelfShadowSecondDensityScale;

	/** Controls the strength of the second self shadow gradient. */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Second Opacity"))
	float TranslucentSelfShadowSecondOpacity;

	/** 
	 * Controls how diffuse the material's backscattering is when using the MSM_Subsurface shading model.
	 * Larger exponents give a less diffuse look (smaller, brighter backscattering highlight).
	 * This is only used when the object is casting a volumetric translucent shadow from a directional light.
	 */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Backscattering Exponent"))
	float TranslucentBackscatteringExponent;

	/** 
	 * Colored extinction factor used to approximate multiple scattering in dense volumes. 
	 * This is only used when the object is casting a volumetric translucent shadow.
	 */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Multiple Scattering Extinction"))
	FLinearColor TranslucentMultipleScatteringExtinction;

	/** Local space distance to bias the translucent shadow.  Positive values move the shadow away from the light. */
	UPROPERTY(EditAnywhere, Category=TranslucencySelfShadowing, meta=(DisplayName = "Start Offset"))
	float TranslucentShadowStartOffset;

	/** Whether to draw on top of opaque pixels even if behind them. This only has meaning for translucency. */
	UPROPERTY(EditAnywhere, Category=Translucency, AdvancedDisplay)
	uint32 bDisableDepthTest:1;

	/** Whether to generate spherical normals for particles that use this material. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 bGenerateSphericalParticleNormals:1;

	/**
	 * Whether the material takes a tangent space normal or a world space normal as input.
	 * (TangentSpace requires extra instructions but is often more convenient).
	 */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 bTangentSpaceNormal:1;

	/**
	 * If enabled, the material's emissive colour is injected into the LightPropagationVolume
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Material, meta=(DisplayName = "Emissive (Dynamic Area Light)"), AdvancedDisplay)
	uint32 bUseEmissiveForDynamicAreaLighting : 1;

	/**
	 * If enabled, the material's opacity defines how much GI is blocked when using the LightPropagationVolume feature
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Material, meta=(DisplayName = "Block Global Illumination"), AdvancedDisplay)
	uint32 bBlockGI : 1;

	/** 
	 * This is a special usage flag that allows a material to be assignable to any primitive type.
	 * This is useful for materials used by code to implement certain viewmodes, for example the default material or lighting only material.
	 * The cost is that nearly 20x more shaders will be compiled for the material than the average material, which will greatly increase shader compile time and memory usage.
	 * This flag should only be enabled when absolutely necessary, and is purposefully not exposed to the UI to prevent abuse.
	 */
	UPROPERTY(duplicatetransient)
	uint32 bUsedAsSpecialEngineMaterial:1;

	/** 
	 * Indicates that the material and its instances can be use with skeletal meshes.  
	 * This will result in the shaders required to support skeletal meshes being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithSkeletalMesh:1;

	/** 
	 * Indicates that the material and its instances can be use with editor compositing  
	 * This will result in the shaders required to support editor compositing being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithEditorCompositing:1;

	/** 
	 * Indicates that the material and its instances can be use with particle sprites 
	 * This will result in the shaders required to support particle sprites being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithParticleSprites:1;

	/** 
	 * Indicates that the material and its instances can be use with beam trails
	 * This will result in the shaders required to support beam trails being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithBeamTrails:1;

	/** 
	 * Indicates that the material and its instances can be use with mesh particles
	 * This will result in the shaders required to support mesh particles being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithMeshParticles:1;


	/**
	* Indicates that the material and its instances can be use with Niagara sprites (meshes and ribbons, respectively)
	* This will result in the shaders required to support Niagara sprites being compiled which will increase shader compile time and memory usage.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Usage)
	uint32 bUsedWithNiagaraSprites : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Usage)
	uint32 bUsedWithNiagaraRibbons : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Usage)
	uint32 bUsedWithNiagaraMeshParticles : 1;



	/** 
	 * Indicates that the material and its instances can be use with static lighting
	 * This will result in the shaders required to support static lighting being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithStaticLighting:1;

	/** 
	* Indicates that the material and its instances can be use with flex fluid surfaces
	* This will result in the shaders required to support flex fluid surfaces being compiled which will increase shader compile time and memory usage.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Usage)
	uint32 bUsedWithFlexFluidSurfaces : 1;

	/**
	* Indicates that the material and its instances can be use with flex deformable meshes.
	* This will result in the shaders required to support flex deformation being compiled which will increase shader compile time and memory usage.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Usage)
	uint32 bUsedWithFlexMeshes : 1;

	/** 
	 * Indicates that the material and its instances can be use with morph targets
	 * This will result in the shaders required to support morph targets being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithMorphTargets:1;

	/** 
	 * Indicates that the material and its instances can be use with spline meshes
	 * This will result in the shaders required to support spline meshes being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithSplineMeshes:1;

	/** 
	 * Indicates that the material and its instances can be use with instanced static meshes
	 * This will result in the shaders required to support instanced static meshes being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithInstancedStaticMeshes:1;

	/** 
	 * Indicates that the material and its instances can be use with distortion
	 * This will result in the shaders required to support distortion being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY()
	uint32 bUsesDistortion:1;

	/** 
	 * Indicates that the material and its instances can be use with clothing
	 * This will result in the shaders required to support clothing being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage)
	uint32 bUsedWithClothing:1;

	/** 
	 * Indicates that the material and its instances can be use with Slate UI and UMG
	 * This will result in the shaders required to support UI materials being compiled which will increase shader compile time and memory usage.
	 */
	UPROPERTY()
	uint32 bUsedWithUI_DEPRECATED:1;

	/** 
	 * Whether to automatically set usage flags based on what the material is applied to in the editor.
	 * It can be useful to disable this on a base material with many instances, where adding another usage flag accidentally (eg bUsedWithSkeletalMeshes) can add a lot of shader permutations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Usage, AdvancedDisplay)
	uint32 bAutomaticallySetUsageInEditor:1;

	// NVCHANGE_BEGIN: Add VXGI

	/**
	* Indicates that the material and its instances can use VxgiTraceCone function. Only applies to translucent materials.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Translucency)
	uint32 bVxgiConeTracingEnable : 1;

	/**
	* Indicates that the material and its instances can be used with VXGI scene voxelization.
	* This will result in the shaders required to support VXGI being compiled which will increase shader compile time and memory usage.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	uint32 bUsedWithVxgiVoxelization : 1;

	/**
	* Indicates that the material has tessellation enabled in MaterialTessellationMode and should be voxelized with tessellation enabled.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	uint32 bVxgiAllowTesselationDuringVoxelization : 1;

	/**
	* Indicates that any light emitted should be emitted in all directions (useful for emissive light sources). Set this to true for small objects.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	uint32 bVxgiOmniDirectional : 1;

	/**
	* Antialiases the emittance values from this material.
	* When proportional emittance is enabled, the light contribution of each sample covered by emissive objects is distributed between two adjacent voxels in Z direction, proportionally to the position of that sample relative to those voxels.
	* With it enabled, downsampled emittance is not equal to voxelized emittance, and changes are sometimes visible when you move the camera.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	uint32 bVxgiProportionalEmittance : 1;

	/**
	* Controls the use of coverage supersampling for emissive voxelization.
	* On Maxwell, HW conservative raster is used for supersampling, which is fast and the results are accurate;
	* On other GPUs, SW conservative raster by triangle expansion is used, which is much slower.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	uint32 bVxgiCoverageSupersampling : 1;

	/**
	* A multiplier for material sampling rate during emittance voxelization, basically for rasterization resolution.
	* It's useful to use higher sampling rates in case shading results cannot be filtered well enough with once-per-voxel shading.
	* That happens for example with materials that have binary masks over textures or colors, because the mask cannot be filtered
	* with standard texture sampling.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	TEnumAsByte<enum EVxgiMaterialSamplingRate> VxgiMaterialSamplingRate;

	/**
	* When enabled, each covered sample position in Z direction by a pseudo-random offset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI)
	FVector2D VxgiOpacityNoiseScaleBias;

	/**
	* Opacity voxelization thickness in voxels. In the range [0..2].
	* Can be useful to strengthen contact occlusion for solid objects, with a performance hit.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VxgiVoxelizationThickness;

	// NVCHANGE_END: Add VXGI

	/* Forces the material to be completely rough. Saves a number of instructions and one sampler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Material, AdvancedDisplay)
	uint32 bFullyRough:1;

	/** 
	 *	Forces this material to use full (highp) precision in the pixel shader.
	 *	This is slower than the default (mediump) but can be used to work around precision-related rendering errors.
	 *	This setting has no effect on older mobile devices that do not support high precision. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mobile)
	uint32 bUseFullPrecision : 1;

	/* Use lightmap directionality and per pixel normals. If disabled, lighting from lightmaps will be flat but cheaper. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Mobile)
	uint32 bUseLightmapDirectionality:1;

	/* 
	 * Forward renderer: enables multiple parallax-corrected reflection captures that blend together.
	 * Mobile renderer: blend between nearest 3 reflection captures, but reduces the number of samplers available to the material as two more samplers will be used for reflection cubemaps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ForwardShading, meta = (DisplayName = "High Quality Reflections"))
	uint32 bUseHQForwardReflections : 1;

	/* Enables planar reflection when using the forward renderer or mobile. Enabling this setting reduces the number of samplers available to the material as one more sampler will be used for the planar reflection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ForwardShading, meta = (DisplayName = "Planar Reflections"))
	uint32 bUsePlanarForwardReflections : 1;

	/* Reduce roughness based on screen space normal changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Material, AdvancedDisplay)
	uint32 bNormalCurvatureToRoughness : 1;

	/** The type of tessellation to apply to this object.  Note D3D11 required for anything except MTM_NoTessellation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tessellation)
	TEnumAsByte<enum EMaterialTessellationMode> D3D11TessellationMode;

	/** Prevents cracks in the surface of the mesh when using tessellation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tessellation, meta=(DisplayName = "Crack Free Displacement"))
	uint32 bEnableCrackFreeDisplacement:1;

	/** Enables adaptive tessellation, which tries to maintain a uniform number of pixels per triangle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tessellation, meta=(DisplayName = "Adaptive Tessellation"))
	uint32 bEnableAdaptiveTessellation:1;

	UPROPERTY(EditAnywhere, Category=Tessellation)
	float MaxDisplacement;

	/** Enables a wireframe view of the mesh the material is applied to.  */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay)
	uint32 Wireframe:1;

	/** Skips outputting velocity during the base pass. */
	UPROPERTY(EditAnywhere, Category=Material, AdvancedDisplay, meta=(DisplayName = "Support accurate velocities from Vertex Deformation"))
	uint32 bOutputVelocityOnBasePass:1;

	UPROPERTY()
	int32 EditorX;

	UPROPERTY()
	int32 EditorY;

	UPROPERTY()
	int32 EditorPitch;

	UPROPERTY()
	int32 EditorYaw;

	/** Array of material expressions, excluding Comments.  Used by the material editor. */
	UPROPERTY()
	TArray<class UMaterialExpression*> Expressions;

#if WITH_EDITORONLY_DATA
	/** Array of comments associated with this material; viewed in the material editor. */
	UPROPERTY()
	TArray<class UMaterialExpressionComment*> EditorComments;

	/** Controls where this parameter group is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Group Sorting")
	TArray<FParameterGroupData> ParameterGroupData;

#endif // WITH_EDITORONLY_DATA
	/** Array of all functions this material depends on. */
	UPROPERTY()
	TArray<struct FMaterialFunctionInfo> MaterialFunctionInfos;

	/** Array of all parameter collections this material depends on. */
	UPROPERTY()
	TArray<struct FMaterialParameterCollectionInfo> MaterialParameterCollectionInfos;

	/** true if this Material can be assumed Opaque when set to masked. */
	UPROPERTY()
	uint32 bCanMaskedBeAssumedOpaque : 1;

	/** true if Material is masked and uses custom opacity */
	UPROPERTY()
	uint32 bIsMasked_DEPRECATED:1;

	/** true if Material is the preview material used in the material editor. */
	UPROPERTY(transient, duplicatetransient)
	uint32 bIsPreviewMaterial:1;

	/** when true, the material attributes pin is used instead of the regular pins. */
	UPROPERTY(EditAnywhere, Category=Material)
	uint32 bUseMaterialAttributes:1;

	/** When true, translucent materials are fogged. Defaults to true. */
	UPROPERTY(EditAnywhere, Category=Translucency, meta=(DisplayName = "Apply Fogging"))
	uint32 bUseTranslucencyVertexFog:1;

	/** When true, translucent materials have fog computed for every pixel, which costs more but fixes artifacts due to low tessellation. */
	UPROPERTY(EditAnywhere, Category=Translucency)
	uint32 bComputeFogPerPixel:1;

	/** If true the compilation environment will be changed to remove the global COMPILE_SHADERS_FOR_DEVELOPMENT flag. */
	UPROPERTY(transient, duplicatetransient)
	uint32 bAllowDevelopmentShaderCompile:1;

	/** true if this is a special material used for stats by the material editor. */
	UPROPERTY(transient, duplicatetransient)
	uint32 bIsMaterialEditorStatsMaterial:1;

	/** true if we have printed a warning about material usage for a given usage flag. */
	UPROPERTY(transient, duplicatetransient)
	uint32 UsageFlagWarnings;
	
	/** Where the node is inserted in the (post processing) graph, only used if domain is PostProcess */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PostProcessMaterial, meta=(DisplayName = "Blendable Location"))
	TEnumAsByte<enum EBlendableLocation> BlendableLocation;

	/** If multiple nodes with the same  type are inserted at the same point, this defined order and if they get combined, only used if domain is PostProcess */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PostProcessMaterial, meta=(DisplayName = "Blendable Priority"))
	int32 BlendablePriority;

	/** If this is enabled, the blendable will output alpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PostProcessMaterial, meta = (DisplayName = "Output Alpha"))
	bool BlendableOutputAlpha;

	/** Controls how the Refraction input is interpreted and how the refraction offset into scene color is computed for this material. */
	UPROPERTY(EditAnywhere, Category=Refraction)
	TEnumAsByte<enum ERefractionMode> RefractionMode;

	/** This is the refraction depth bias, larger values offset distortion to prevent closer objects from rendering into the distorted surface at acute viewing angles but increases the disconnect between surface and where the refraction starts. */
	UPROPERTY(EditAnywhere, Category=Refraction)
	float RefractionDepthBias;

	/** 
	 * Guid that uniquely identifies this material. 
	 * Any changes to the state of the material that do not appear separately in the shadermap DDC keys must cause this guid to be regenerated!
	 * For example, a modification to the Expressions array.
	 * Code changes that cause the guid to be regenerated on load should be avoided, as that requires a resave of the content to stop recompiling every load.
	 */
	UPROPERTY()
	FGuid StateId;

	/** 
	 * FMaterialRenderProxy derivatives that represent this material to the renderer, when the renderer needs to fetch parameter values. 
	 * Second instance is used when selected, third when hovered.
	 */
	class FDefaultMaterialInstance* DefaultMaterialInstances[3];

	/** Used to detect duplicate parameters.  Does not contain parameters in referenced functions! */
	TMap<FName, TArray<UMaterialExpression*> > EditorParameters;

#if WITH_EDITORONLY_DATA
	/** EdGraph based representation of the Material */
	class UMaterialGraph*	MaterialGraph;
#endif //WITH_EDITORONLY_DATA

private:

	/** Inline material resources serialized from disk. To be processed on game thread in PostLoad. */
	TArray<FMaterialResource> LoadedMaterialResources;

	/** 
	 * Material resources used for rendering this material.
	 * There need to be as many entries in this array as can be used simultaneously for rendering.  
	 * For example the material needs to support being rendered at different quality levels and feature levels within the same process.
	 * These are always valid and non-null, but only the entries affected by CacheResourceShadersForRendering are actually valid for rendering.
	 */
	FMaterialResource* MaterialResources[EMaterialQualityLevel::Num][ERHIFeatureLevel::Num];
#if WITH_EDITOR
	/** Material resources being cached for cooking. */
	TMap<const class ITargetPlatform*, TArray<FMaterialResource*>> CachedMaterialResourcesForCooking;
#endif
	/** Fence used to guarantee that the RT is finished using various resources in this UMaterial before cleanup. */
	FRenderCommandFence ReleaseFence;

	/** 
	 * Cached texture references from all expressions in the material (including nested functions). 
	 * This is used to link uniform texture expressions which were stored in the DDC with the UTextures that they reference.
	 */
	UPROPERTY(transient)
	TArray<UTexture*> ExpressionTextureReferences;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FGuid> ReferencedTextureGuids;

#endif // WITH_EDITORONLY_DATA
public:

	//~ Begin UMaterialInterface Interface.
	ENGINE_API virtual UMaterial* GetMaterial() override;
	ENGINE_API virtual const UMaterial* GetMaterial() const override;
	ENGINE_API virtual const UMaterial* GetMaterial_Concurrent(TMicRecursionGuard& RecursionGuard) const override;
	ENGINE_API virtual bool GetParameterDesc(FName ParameterName, FString& OutDesc) const override;
	ENGINE_API virtual bool GetVectorParameterValue(FName ParameterName,FLinearColor& OutValue) const override;
	ENGINE_API virtual bool GetScalarParameterValue(FName ParameterName,float& OutValue) const override;
	ENGINE_API virtual bool GetTextureParameterValue(FName ParameterName,class UTexture*& OutValue) const override;
	ENGINE_API virtual bool GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue,int32& OutFontPage) const override;
	ENGINE_API virtual bool GetGroupName(FName ParameterName, FName& OutDesc) const override;
	ENGINE_API virtual bool GetRefractionSettings(float& OutBiasValue) const override;
	ENGINE_API virtual FMaterialRenderProxy* GetRenderProxy(bool Selected, bool bHovered=false) const override;
	ENGINE_API virtual UPhysicalMaterial* GetPhysicalMaterial() const override;
	ENGINE_API virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const override;
	ENGINE_API virtual void GetUsedTexturesAndIndices(TArray<UTexture*>& OutTextures, TArray< TArray<int32> >& OutIndices, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) const override;
	ENGINE_API virtual void OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture, ERHIFeatureLevel::Type InFeatureLevel ) override;
	ENGINE_API virtual void OverrideVectorParameterDefault(FName ParameterName, const FLinearColor& Value, bool bOverride, ERHIFeatureLevel::Type FeatureLevel) override;
	ENGINE_API virtual void OverrideScalarParameterDefault(FName ParameterName, float Value, bool bOverride, ERHIFeatureLevel::Type FeatureLevel) override;
	ENGINE_API virtual float GetScalarParameterDefault(FName ParameterName, ERHIFeatureLevel::Type FeatureLevel) override;
	ENGINE_API virtual bool CheckMaterialUsage(const EMaterialUsage Usage) override;
	ENGINE_API virtual bool CheckMaterialUsage_Concurrent(const EMaterialUsage Usage) const override;
	ENGINE_API virtual FMaterialResource* AllocateResource();
	ENGINE_API virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) override;
	ENGINE_API virtual const FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const override;
	ENGINE_API virtual bool GetStaticSwitchParameterValue(FName ParameterName,bool &OutValue,FGuid &OutExpressionGuid) const override;
	ENGINE_API virtual bool GetStaticComponentMaskParameterValue(FName ParameterName, bool &R, bool &G, bool &B, bool &A, FGuid &OutExpressionGuid) const override;
	ENGINE_API virtual bool GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid) const override;
	ENGINE_API virtual bool UpdateLightmassTextureTracking() override;
#if WITH_EDITOR
	ENGINE_API virtual bool GetParameterSortPriority(FName ParameterName, int32& OutSortPriority) const override;
	ENGINE_API virtual bool GetGroupSortPriority(const FString& InGroupName, int32& OutSortPriority) const override;
	ENGINE_API virtual bool GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures, 
		TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet) override;
#endif
	ENGINE_API virtual void RecacheUniformExpressions() const override;

	ENGINE_API virtual float GetOpacityMaskClipValue() const override;
	ENGINE_API virtual bool GetCastDynamicShadowAsMasked() const override;
	ENGINE_API virtual EBlendMode GetBlendMode() const override;
	ENGINE_API virtual EMaterialShadingModel GetShadingModel() const override;
	ENGINE_API virtual bool IsTwoSided() const override;
	ENGINE_API virtual bool IsDitheredLODTransition() const override;
	ENGINE_API virtual bool IsTranslucencyWritingCustomDepth() const override;
	ENGINE_API virtual bool IsMasked() const override;
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	ENGINE_API virtual FVxgiMaterialProperties GetVxgiMaterialProperties() const override;
#endif
	// NVCHANGE_END: Add VXGI
	ENGINE_API virtual bool IsUIMaterial() const { return MaterialDomain == MD_UI; }
	ENGINE_API virtual bool IsPostProcessMaterial() const { return MaterialDomain == MD_PostProcess; }
	ENGINE_API virtual USubsurfaceProfile* GetSubsurfaceProfile_Internal() const override;

	ENGINE_API void SetShadingModel(EMaterialShadingModel NewModel) {ShadingModel = NewModel;}
	ENGINE_API bool GetScalarParameterSliderMinMax(FName ParameterName, float& OutMinSlider, float& OutMaxSlider) const;

	/** Checks to see if an input property should be active, based on the state of the material */
	ENGINE_API virtual bool IsPropertyActive(EMaterialProperty InProperty) const override;
#if WITH_EDITOR
	/** Allows material properties to be compiled with the option of being overridden by the material attributes input. */
	ENGINE_API virtual int32 CompilePropertyEx( class FMaterialCompiler* Compiler, const FGuid& AttributeID ) override;
#endif // WITH_EDITOR
	ENGINE_API virtual void ForceRecompileForRendering() override;
	//~ End UMaterialInterface Interface.

	//~ Begin UObject Interface
	ENGINE_API virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITOR
	ENGINE_API virtual void BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform ) override;
	ENGINE_API virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) override;
	ENGINE_API virtual void ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform ) override;
	ENGINE_API virtual void ClearAllCachedCookedPlatformData() override;
#endif
#if WITH_EDITOR
	ENGINE_API virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	ENGINE_API virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif // WITH_EDITOR
	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;
	ENGINE_API virtual void FinishDestroy() override;
	ENGINE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	ENGINE_API virtual bool CanBeClusterRoot() const override;
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface

#if WITH_EDITOR
	/** Cancels any currently outstanding compilation jobs for this material. Useful in the material editor when some edits superceds existing, in flight compilation jobs.*/
	ENGINE_API virtual void CancelOutstandingCompilation();
#endif // WITH_EDITOR

	/**
	 * Return the default material, loading it if necessary
	 */
	ENGINE_API static UMaterial* GetDefaultMaterial(EMaterialDomain Domain);

	/**
	 * Returns true if the material is one of the default materials.
	 */
	ENGINE_API bool IsDefaultMaterial() const;

	/** 
	 * Releases rendering resources used by this material. 
	 * This should only be called directly if the material will not be deleted through the GC system afterward.
	 * FlushRenderingCommands() must have been called before this.
	 */
	ENGINE_API void ReleaseResources();

	/** Checks to see if the Usage flag has an annotation marking it as needing to be saved */
	ENGINE_API bool IsUsageFlagDirty(EMaterialUsage Usage);
	
	/** Useful to customize rendering if that case (e.g. hide the object) */
	ENGINE_API bool IsCompilingOrHadCompileError(ERHIFeatureLevel::Type InFeatureLevel);


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Output to the log which materials and textures are used by this material.
	 * @param Indent	Number of tabs to put before the log.
	 */
	ENGINE_API virtual void LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const override;
#endif

	/**
	 *	Returns all the Guids related to this material. For material instances, this includes the parent hierarchy.
	 *  Used for versioning as parent changes don't update the child instance Guids.
	 *
	 *	@param	bIncludeTextures	Whether to include the referenced texture Guids.
	 *	@param	OutGuids			The list of all resource guids affecting the precomputed lighting system and texture streamer.
	 */
	ENGINE_API virtual void GetLightingGuidChain(bool bIncludeTextures, TArray<FGuid>& OutGuids) const override;

private:
	void BackwardsCompatibilityInputConversion();

	/** Handles setting up an annotation for this object if a flag has changed value */
	void MarkUsageFlagDirty(EMaterialUsage Usage, bool CurrentValue, bool NewValue);

	/** Sets the value associated with the given usage flag. */
	void SetUsageByFlag(const EMaterialUsage Usage, const bool NewValue);

	/** Sets up transient properties in MaterialResources. */
	void UpdateResourceAllocations();

	/** to share code for PostLoad() and PostEditChangeProperty(), and UMaterialInstance::InitResources(), needs to be refactored */
	void PropagateDataToMaterialProxy();

#if WITH_EDITOR
	/** Marks the material's package dirty in order to make a material usage change set during map load persistent. 
	  * This couldn't be done during map load as loading cannot mark packages dirty. Invoked manually by the user 
	  * from the Map Check message log.
	  */
	void FixupMaterialUsageAfterLoad();
#endif

public:

	/** @return the name of the given usage flag. */
	FString GetUsageName(const EMaterialUsage Usage) const;

	/** @return the value associated with the given usage flag. */
	ENGINE_API bool GetUsageByFlag(const EMaterialUsage Usage) const;


	/**
	 * Set the given usage flag.
	 * @param bNeedsRecompile - true if the material was recompiled for the usage change
	 * @param Usage - The usage flag to set
	 * @return bool - true if the material can be used for rendering with the given type.
	 */
	ENGINE_API bool SetMaterialUsage(bool &bNeedsRecompile, const EMaterialUsage Usage);

	/**
	 * Tests to see if this material needs a usage flag update
	 * @param Usage - The usage flag to set
	 * @param bOutHasUsage - if we don't need to call SMU, then this is what SMU would have returned
	 * @return bool - true if we need to call SetMaterialUsage
	 */
	ENGINE_API bool NeedsSetMaterialUsage_Concurrent(bool &bOutHasUsage, const EMaterialUsage Usage) const;

	/**
	 * @param	OutParameterNames		Storage array for the parameter names we are returning.
	 * @param	OutParameterIds			Storage array for the parameter id's we are returning.
	 *
	 * @return	Returns a array of parameter names used in this material for the specified expression type.
	 */
	template<typename ExpressionType>
	void GetAllParameterNames(TArray<FName>& OutParameterNames, TArray<FGuid>& OutParameterIds) const
	{
		for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
		{
			const UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<const UMaterialExpressionMaterialFunctionCall>(Expressions[ExpressionIndex]);

			if (FunctionExpression)
			{
				if (FunctionExpression->MaterialFunction)
				{
					FunctionExpression->MaterialFunction->GetAllParameterNames<ExpressionType>(OutParameterNames, OutParameterIds);
				}
			}
			else
			{
				const ExpressionType* ParameterExpression = Cast<const ExpressionType>(Expressions[ExpressionIndex]);

				if (ParameterExpression)
				{
					ParameterExpression->GetAllParameterNames(OutParameterNames, OutParameterIds);
				}
			}
		}

		check(OutParameterNames.Num() == OutParameterIds.Num());
	}
	
	ENGINE_API void GetAllVectorParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
	ENGINE_API void GetAllScalarParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
	ENGINE_API void GetAllTextureParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
	ENGINE_API void GetAllFontParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
	ENGINE_API void GetAllStaticSwitchParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
	ENGINE_API void GetAllStaticComponentMaskParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;

	/** Returns the material's decal blend mode, calculated from the DecalBlendMode property and what inputs are connected. */
	uint32 GetDecalBlendMode() const { return DecalBlendMode; }

	/** Returns the material's decal response mode */
	uint32 GetMaterialDecalResponse() const { return MaterialDecalResponse; }

	/**
	 * Attempt to find a expression by its GUID.
	 */
	template<typename ExpressionType>
	ExpressionType* FindExpressionByGUID(const FGuid &InGUID)
	{
		return FindExpressionByGUIDRecursive<ExpressionType>(InGUID, Expressions);
	}

	/* Get all expressions of the requested type */
	template<typename ExpressionType>
	void GetAllExpressionsOfType(TArray<const ExpressionType*>& OutExpressions) const
	{
		for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
		{
			ExpressionType* ExpressionPtr = Cast<ExpressionType>(Expressions[ExpressionIndex]);
			if (ExpressionPtr)
			{
				OutExpressions.Add(ExpressionPtr);
			}
		}
	}

	/** Get all expressions of the requested type, recursing through any function expressions in the material */
	template<typename ExpressionType>
	void GetAllExpressionsInMaterialAndFunctionsOfType(TArray<ExpressionType*>& OutExpressions) const
	{
		for (UMaterialExpression* Expression : Expressions)
		{
			ExpressionType* ExpressionOfType = Cast<ExpressionType>(Expression);
			if (ExpressionOfType)
			{
				OutExpressions.Add(ExpressionOfType);
			}

			UMaterialExpressionMaterialFunctionCall* ExpressionFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
			if (ExpressionFunctionCall && ExpressionFunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(ExpressionFunctionCall->MaterialFunction);

				ExpressionFunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				// Handle nested functions
				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						ExpressionType* FunctionExpressionOfType = Cast<ExpressionType>(FunctionExpression);
						if (FunctionExpressionOfType)
						{
							OutExpressions.Add(FunctionExpressionOfType);
						}
					}
				}
			}
		}
	}

	/** Checks if the material contains an expression of the requested type, recursing through any function expressions in the material */
	template<typename ExpressionType>
	bool HasAnyExpressionsInMaterialAndFunctionsOfType() const
	{
		for (UMaterialExpression* Expression : Expressions)
		{
			ExpressionType* ExpressionOfType = Cast<ExpressionType>(Expression);
			if (ExpressionOfType)
			{
				return true;
			}

			UMaterialExpressionMaterialFunctionCall* ExpressionFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
			if (ExpressionFunctionCall && ExpressionFunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(ExpressionFunctionCall->MaterialFunction);

				ExpressionFunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				// Handle nested functions
				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						ExpressionType* FunctionExpressionOfType = Cast<ExpressionType>(FunctionExpression);
						if (FunctionExpressionOfType)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	/** Determines whether each quality level has different nodes by inspecting the material's expressions. 
	* Or is required by the material quality setting overrides.
	* @param	QualityLevelsUsed	output array of used quality levels.
	* @param	ShaderPlatform	The shader platform to use for the quality settings.
	*/
	void GetQualityLevelUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& QualityLevelsUsed, EShaderPlatform ShaderPlatform);

	/** Determines whether each quality level has different nodes by inspecting the material's expressions.
	* @param	QualityLevelsUsed	output array of used quality levels.
	*/
	void GetQualityLevelNodeUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& OutQualityLevelsUsed);

	/**
	 * Cache the expression texture references for this UMaterial 
	 * if the cache is not filled then it will rebuild the texture references
	 * see also RebuildExpressionTextureReferences
	 */
	void CacheExpressionTextureReferences();

	/** Rebuild ExpressionTextureReferences with all textures referenced by expressions in this material. */
	void RebuildExpressionTextureReferences();

	/** Attempts to add a new group name to the Group Data struct. True if new name was added. */
	ENGINE_API bool AttemptInsertNewGroupName(const FString& InNewName);

private:
	/**
	 * Flush existing resource shader maps and resets the material resource's Ids.
	 */
	ENGINE_API virtual void FlushResourceShaderMaps();

	/** 
	 * Rebuild the MaterialFunctionInfos array with the current state of the material's function dependencies,
	 * And updates any function call nodes in this material so their inputs and outputs stay valid.
	 */
	void RebuildMaterialFunctionInfo();

	/** 
	 * Rebuild the MaterialParameterCollectionInfos array with the current state of the material's parameter collection dependencies.
	 */
	void RebuildMaterialParameterCollectionInfo();
	

	/** 
	 * Cache resource shaders for rendering. 
	 * If a matching shader map is not found in memory or the DDC, a new one will be compiled.
	 * The results will be applied to this FMaterial in the renderer when they are finished compiling.
	 * Note: This modifies material variables used for rendering and is assumed to be called within a FMaterialUpdateContext!
	 */
	void CacheResourceShadersForRendering(bool bRegenerateId);

	/**
	 * Cache resource shaders for cooking on the given shader platform.
	 * If a matching shader map is not found in memory or the DDC, a new one will be compiled.
	 * This does not apply completed results to the renderer scenes.
	 * Caller is responsible for deleting OutCachedMaterialResources.
	 * Note: This modifies material variables used for rendering and is assumed to be called within a FMaterialUpdateContext!
	 */
	void CacheResourceShadersForCooking(EShaderPlatform Platform, TArray<FMaterialResource*>& OutCachedMaterialResources);

	/** Caches shader maps for an array of material resources. */
	void CacheShadersForResources(EShaderPlatform ShaderPlatform, const TArray<FMaterialResource*>& ResourcesToCache, bool bApplyCompletedShaderMapForRendering);

	/**
	 * If there is some texture reference used by a TextureProperty node in any expressions, this function
	 * will extract the current hash of TextureReferencesHash into a string  then append the texture guid used by the node
	 * and recompute a new hash.
	 */
	void GetForceRecompileTextureIdsHash(FSHAHash &TextureReferencesHash);

public:
	bool IsTextureForceRecompileCacheRessource(UTexture *Texture);

#if WITH_EDITOR
	/* Recompute the ddc cache key and reload the material in case the key is not the same.
	 * It will also make sure lightmass texture reference are up to date
	 */
	void UpdateMaterialShaderCacheAndTextureReferences();
#endif

	/**
	 * Go through every material, flush the specified types and re-initialize the material's shader maps.
	 */
	ENGINE_API static void UpdateMaterialShaders(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FShaderPipelineType*>& ShaderPipelineTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush, EShaderPlatform ShaderPlatform);

	/** 
	 * Backs up all material shaders to memory through serialization, organized by FMaterialShaderMap. 
	 * This will also clear all FMaterialShaderMap references to FShaders.
	 */
	ENGINE_API static void BackupMaterialShadersToMemory(TMap<class FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData);

	/** 
	 * Recreates FShaders for FMaterialShaderMap's from the serialized data.  Shader maps may not be complete after this due to changes in the shader keys.
	 */
	ENGINE_API static void RestoreMaterialShadersFromMemory(const TMap<class FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData);

	/** Builds a map from UMaterialInterface name to the shader maps that are needed for rendering on the given platform. */
	ENGINE_API static void CompileMaterialsForRemoteRecompile(
		const TArray<UMaterialInterface*>& MaterialsToCompile,
		EShaderPlatform ShaderPlatform, 
		TMap<FString, TArray<TRefCountPtr<class FMaterialShaderMap> > >& OutShaderMaps);

	/**
	 * Add an expression node that represents a parameter to the list of material parameters.
	 * @param	Expression	Pointer to the node that is going to be inserted if it's a parameter type.
	 */
	ENGINE_API virtual bool AddExpressionParameter(UMaterialExpression* Expression, TMap<FName, TArray<UMaterialExpression*> >& ParameterTypeMap);

	/**
	 * Removes an expression node that represents a parameter from the list of material parameters.
	 * @param	Expression	Pointer to the node that is going to be removed if it's a parameter type.
	 */
	ENGINE_API virtual bool RemoveExpressionParameter(UMaterialExpression* Expression);

	/**
	 * A parameter with duplicates has to update its peers so that they all have the same value. If this step isn't performed then
	 * the expression nodes will not accurately display the final compiled material.
	 * @param	Parameter	Pointer to the expression node whose state needs to be propagated.
	 */
	ENGINE_API virtual void PropagateExpressionParameterChanges(UMaterialExpression* Parameter);

	/**
	 * Remove the expression from the editor parameters list (if it exists) and then re-adds it.
	 * @param	Expression	The expression node that represents a parameter that needs updating.
	 */
	ENGINE_API virtual void UpdateExpressionParameterName(UMaterialExpression* Expression);

	/**
	 * Iterate through all of the expression nodes in the material and finds any parameters to put in EditorParameters.
	 */
	ENGINE_API virtual void BuildEditorParameterList();

	/**
	 * Return whether the provided expression parameter has duplicates.
	 * @param	Expression	The expression parameter to check for duplicates.
	 */
	ENGINE_API virtual bool HasDuplicateParameters(const UMaterialExpression* Expression);

	/**
	 * Return whether the provided expression dynamic parameter has duplicates.
	 * @param	Expression	The expression dynamic parameter to check for duplicates.
	 */
	ENGINE_API virtual bool HasDuplicateDynamicParameters(const UMaterialExpression* Expression);

	/**
	 * Iterate through all of the expression nodes and fix up changed properties on
	 * matching dynamic parameters when a change occurs.
	 *
	 * @param	Expression	The expression dynamic parameter.
	 */
	ENGINE_API virtual void UpdateExpressionDynamicParameters(const UMaterialExpression* Expression);

	/**
	 * Get the name of a parameter.
	 * @param	Expression	The expression to retrieve the name from.
	 * @param	OutName		The variable that will hold the parameter name.
	 * @return	true if the expression is a parameter with a name.
	 */
	static bool GetExpressionParameterName(const UMaterialExpression* Expression, FName& OutName);

	/**
	 * Copy the values of an expression parameter to another expression parameter of the same class.
	 *
	 * @param	Source			The source parameter.
	 * @param	Destination		The destination parameter that will receive Source's values.
	 */
	static bool CopyExpressionParameters(UMaterialExpression* Source, UMaterialExpression* Destination);

	/**
	 * Return whether the provided expression node is a parameter.
	 *
	 * @param	Expression	The expression node to inspect.
	 */
	ENGINE_API static bool IsParameter(const UMaterialExpression* Expression);

	/**
	 * Return whether the provided expression node is a dynamic parameter.
	 *
	 * @param	Expression	The expression node to inspect.
	 */
	ENGINE_API static bool IsDynamicParameter(const UMaterialExpression* Expression);

	/** Returns an array of the guids of functions used in this material, with the call hierarchy flattened. */
	void AppendReferencedFunctionIdsTo(TArray<FGuid>& OutIds) const;

	/** Returns an array of the guids of parameter collections used in this material. */
	void AppendReferencedParameterCollectionIdsTo(TArray<FGuid>& OutIds) const;

	/* Helper functions for text output of properties. */
	static const TCHAR* GetMaterialShadingModelString(EMaterialShadingModel InMaterialShadingModel);
	static EMaterialShadingModel GetMaterialShadingModelFromString(const TCHAR* InMaterialShadingModelStr);
	static const TCHAR* GetBlendModeString(EBlendMode InBlendMode);
	static EBlendMode GetBlendModeFromString(const TCHAR* InBlendModeStr);

	/**
	*	Get the expression input for the given property
	*
	*	@param	InProperty				The material property chain to inspect, such as MP_BaseColor.
	*
	*	@return	FExpressionInput*		A pointer to the expression input of the property specified, 
	*									or NULL if an invalid property was requested (some properties have been removed from UI, those return NULL).
	*/
	ENGINE_API FExpressionInput* GetExpressionInputForProperty(EMaterialProperty InProperty);

	/* Returns any UMaterialExpressionCustomOutput expressions */
	ENGINE_API void GetAllCustomOutputExpressions(TArray<class UMaterialExpressionCustomOutput*>& OutCustomOutputs) const;
	ENGINE_API void GetAllExpressionsForCustomInterpolators(TArray<class UMaterialExpression*>& OutExpressions) const;

#if WITH_EDITOR
	/**
	 *	Get all referenced expressions (returns the chains for all properties).
	 *
	 *	@param	OutExpressions			The array to fill in all of the expressions.
	 *	@param	InStaticParameterSet	Optional static parameter set - if supplied only walk the StaticSwitch branches according to it.
	 *
	 *	@return	bool					true if successful, false if not.
	 */
	ENGINE_API virtual bool GetAllReferencedExpressions(TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);

	/**
	 *	Get the expression chain for the given property (ie fill in the given array with all expressions in the chain).
	 *
	 *	@param	InProperty				The material property chain to inspect, such as MP_BaseColor.
	 *	@param	OutExpressions			The array to fill in all of the expressions.
	 *	@param	InStaticParameterSet	Optional static parameter set - if supplied only walk the StaticSwitch branches according to it.
	 *
	 *	@return	bool					true if successful, false if not.
	 */
	ENGINE_API virtual bool GetExpressionsInPropertyChain(EMaterialProperty InProperty, 
		TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);
#endif

	/** Appends textures referenced by expressions, including nested functions. */
	ENGINE_API void AppendReferencedTextures(TArray<UTexture*>& InOutTextures) const;

protected:

#if WITH_EDITOR
	/**
	 *	Recursively retrieve the expressions contained in the chain of the given expression.
	 *
	 *	@param	InExpression			The expression to start at.
	 *	@param	InOutProcessedInputs	An array of processed expression inputs. (To avoid circular loops causing infinite recursion)
	 *	@param	OutExpressions			The array to fill in all of the expressions.
	 *	@param	InStaticParameterSet	Optional static parameter set - if supplied only walk the StaticSwitch branches according to it.
	 *
	 *	@return	bool					true if successful, false if not.
	 */
	ENGINE_API virtual bool RecursiveGetExpressionChain(UMaterialExpression* InExpression, TArray<FExpressionInput*>& InOutProcessedInputs, 
		TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);

	/**
	*	Recursively update the bRealtimePreview for each expression based on whether it is connected to something that is time-varying.
	*	This is determined based on the result of UMaterialExpression::NeedsRealtimePreview();
	*
	*	@param	InExpression				The expression to start at.
	*	@param	InOutExpressionsToProcess	Array of expressions we still need to process.
	*
	*/
	void RecursiveUpdateRealtimePreview(UMaterialExpression* InExpression, TArray<UMaterialExpression*>& InOutExpressionsToProcess);
#endif

public:
	bool HasNormalConnected() const { return Normal.IsConnected(); }

	static void NotifyCompilationFinished(UMaterialInterface* Material);

	DECLARE_EVENT_OneParam( UMaterial, FMaterialCompilationFinished, UMaterialInterface* );
	ENGINE_API static FMaterialCompilationFinished& OnMaterialCompilationFinished();

	// For all materials, UMaterial::CacheResourceShadersForRendering
	ENGINE_API static void AllMaterialsCacheResourceShadersForRendering();

#if WITH_EDITORONLY_DATA
	/**
	 * Flip the X coordinates of a material's expressions and space them out more
	 *
	 * @param	Expressions		Array of material expressions
	 * @param	Comments		Array of material expression comments
	 * @param	bScaleCoords	Whether to scale the coordinates to space out nodes
	 * @param	Material		The Material to flip its home coords (optional)
	 */
	static void FlipExpressionPositions(const TArray<UMaterialExpression*>& Expressions, const TArray<UMaterialExpressionComment*>& Comments, bool bScaleCoords, UMaterial* Material = NULL);

	/**
	 * Shifts the positions of comments so that they are aligned correctly with other expressions
	 *
	 * @param	Comments	Array of comments to fix
	 */
	static void FixCommentPositions(const TArray<UMaterialExpressionComment*>& Comments);

	/**
	 * Checks whether a Material is arranged in the old style, with inputs flowing from right to left
	 */
	bool HasFlippedCoordinates();
#endif //WITH_EDITORONLY_DATA

private:
	static FMaterialCompilationFinished MaterialCompilationFinishedEvent;

	friend class FLightmassMaterialProxy;
	/** Class that knows how to update Materials */
	friend class FMaterialUpdateContext;
	friend class FMaterialResource;
	friend class FMaterialEditor;

	// DO NOT CALL outside of FMaterialEditor!
	ENGINE_API static void ForceNoCompilationInPostLoad(bool bForceNoCompilation);

	/* Helper function to help finding expression GUID taking into account UMaterialExpressionMaterialFunctionCall */
	template<typename ExpressionType>
	ExpressionType* FindExpressionByGUIDRecursive(const FGuid &InGUID, const TArray<UMaterialExpression*>& InMaterialExpression)
	{
		for (int32 ExpressionIndex = 0; ExpressionIndex < InMaterialExpression.Num(); ExpressionIndex++)
		{
			UMaterialExpression* ExpressionPtr = InMaterialExpression[ExpressionIndex];
			UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(ExpressionPtr);

			if (MaterialFunctionCall != nullptr)
			{
				if (MaterialFunctionCall->MaterialFunction != nullptr)
				{
					return FindExpressionByGUIDRecursive<ExpressionType>(InGUID, MaterialFunctionCall->MaterialFunction->FunctionExpressions);
				}
			}
			else if (ExpressionPtr && ExpressionPtr->bIsParameterExpression && ExpressionPtr->GetParameterExpressionId() == InGUID)
			{
				return Cast<ExpressionType>(ExpressionPtr);
			}
		}

		return nullptr;
	}

};



