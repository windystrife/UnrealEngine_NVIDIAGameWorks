// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RenderCommandFence.h"
#include "EngineDefines.h"
#include "SceneTypes.h"
#include "RenderResource.h"
#include "Components/LightComponentBase.h"
#include "LightComponent.generated.h"

class FLightComponentMapBuildData;
class FStaticShadowDepthMapData;
class ULevel;
class UMaterialInterface;
class UPrimitiveComponent;
class UTextureLightProfile;


// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
UENUM()
namespace ETessellationQuality
{
	enum Type
	{
		LOW,
		MEDIUM,
		HIGH,
	};
}
// NVCHANGE_END: Nvidia Volumetric Lighting

/** 
 * A texture containing depth values of static objects that was computed during the lighting build.
 * Used by Stationary lights to shadow translucency.
 */
class FStaticShadowDepthMap : public FTexture
{
public:

	FStaticShadowDepthMap() : 
		Data(NULL)
	{}

	const FStaticShadowDepthMapData* Data;

	virtual void InitRHI();
};

UCLASS(abstract, HideCategories=(Trigger,Activation,"Components|Activation",Physics), ShowCategories=(Mobility))
class ENGINE_API ULightComponent : public ULightComponentBase
{
	GENERATED_UCLASS_BODY()

	/**
	* Color temperature in Kelvin of the blackbody illuminant.
	* White (D65) is 6500K.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "1700.0", UIMax = "12000.0"))
	float Temperature;
	
	UPROPERTY(EditAnywhere, Category = Performance)
	float MaxDrawDistance;

	UPROPERTY(EditAnywhere, Category = Performance)
	float MaxDistanceFadeRange;

	/** false: use white (D65) as illuminant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(DisplayName = "Use Temperature"))
	uint32 bUseTemperature : 1;

	/** 
	 * Legacy shadowmap channel from the lighting build, now stored in FLightComponentMapBuildData.
	 */
	UPROPERTY()
	int32 ShadowMapChannel_DEPRECATED;

	/** Transient shadowmap channel used to preview the results of stationary light shadowmap packing. */
	int32 PreviewShadowMapChannel;
	
	/** Min roughness effective for this light. Used for softening specular highlights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(UIMin = "0.08", UIMax = "1.0"))
	float MinRoughness;

	/** 
	 * Scales the resolution of shadowmaps used to shadow this light.  By default shadowmap resolution is chosen based on screen size of the caster. 
	 * Note: shadowmap resolution is still clamped by 'r.Shadow.MaxResolution'
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(UIMin = ".125", UIMax = "8"))
	float ShadowResolutionScale;

	/** 
	 * Controls how accurate self shadowing of whole scene shadows from this light are.  
	 * At 0, shadows will start at the their caster surface, but there will be many self shadowing artifacts.
	 * larger values, shadows will start further from their caster, and there won't be self shadowing artifacts but object might appear to fly.
	 * around 0.5 seems to be a good tradeoff. This also affects the soft transition of shadows
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(UIMin = "0", UIMax = "1"))
	float ShadowBias;

	/** Amount to sharpen shadow filtering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", DisplayName = "Shadow Filter Sharpen"))
	float ShadowSharpen;
	
	/** Length of screen space ray trace for sharp contact shadows. Zero is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light, AdvancedDisplay, meta = (UIMin = "0.0", UIMax = "1.0"))
	float ContactShadowLength;

	UPROPERTY()
	uint32 InverseSquaredFalloff_DEPRECATED:1;

	/** Whether the light is allowed to cast dynamic shadows from translucency. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 CastTranslucentShadows:1;

	/** 
	 * Whether the light should only cast shadows from components marked as bCastCinematicShadows. 
	 * This is useful for setting up cinematic Movable spotlights aimed at characters and avoiding the shadow depth rendering costs of the background.
	 * Note: this only works with dynamic shadow maps, not with static shadowing or Ray Traced Distance Field shadows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 bCastShadowsFromCinematicObjectsOnly:1;

	/**
	 * Whether the light should be injected into the Light Propagation Volume
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(DisplayName = "Dynamic Indirect Lighting"))
	uint32 bAffectDynamicIndirectLighting : 1;

	/** 
	 * Channels that this light should affect.  
	 * These channels only apply to opaque materials, direct lighting, and dynamic lighting and shadowing.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Light)
	FLightingChannels LightingChannels;

	/** 
	 * The light function material to be applied to this light.
	 * Note that only non-lightmapped lights (UseDirectLightMap=False) can have a light function. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightFunction)
	class UMaterialInterface* LightFunctionMaterial;

	/** Scales the light function projection.  X and Y scale in the directions perpendicular to the light's direction, Z scales along the light direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightFunction, meta=(AllowPreserveRatio = "true"))
	FVector LightFunctionScale;

	/** IES texture (light profiles from real world measured data) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightProfiles, meta=(DisplayName = "IES Texture"))
	class UTextureLightProfile* IESTexture;

	/** true: take light brightness from IES profile, false: use the light brightness - the maximum light in one direction is used to define no masking. Use with InverseSquareFalloff. Will be disabled if a valid IES profile texture is not supplied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightProfiles, meta=(DisplayName = "Use IES Intensity"))
	uint32 bUseIESBrightness : 1;

	/** Global scale for IES brightness contribution. Only available when "Use IES Brightness" is selected, and a valid IES profile texture is set */
	UPROPERTY(BlueprintReadOnly, interp, Category=LightProfiles, meta=(UIMin = "0.0", UIMax = "10.0", DisplayName = "IES Intensity Scale"))
	float IESBrightnessScale;

	/** 
	 * Distance at which the light function should be completely faded to DisabledBrightness.  
	 * This is useful for hiding aliasing from light functions applied in the distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightFunction, meta=(DisplayName = "Fade Distance"))
	float LightFunctionFadeDistance;

	/** 
	 * Brightness factor applied to the light when the light function is specified but disabled, for example in scene captures that use SceneCapView_LitNoShadows. 
	 * This should be set to the average brightness of the light function material's emissive input, which should be between 0 and 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightFunction, meta=(UIMin = "0.0", UIMax = "1.0"))
	float DisabledBrightness;

	/** 
	 * Whether to render light shaft bloom from this light. 
	 * For directional lights, the color around the light direction will be blurred radially and added back to the scene.
	 * for point lights, the color on pixels closer than the light's SourceRadius will be blurred radially and added back to the scene.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts, meta=(DisplayName = "Light Shaft Bloom"))
	uint32 bEnableLightShaftBloom:1;

	/** Scales the additive color. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts, meta=(UIMin = "0", UIMax = "10"))
	float BloomScale;

	/** Scene color must be larger than this to create bloom in the light shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts, meta=(UIMin = "0", UIMax = "4"))
	float BloomThreshold;

	/** Multiplies against scene color to create the bloom color. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LightShafts)
	FColor BloomTint;

	/** 
	 * Whether to use ray traced distance field area shadows.  The project setting bGenerateMeshDistanceFields must be enabled for this to have effect.
	 * Distance field shadows support area lights so they create soft shadows with sharp contacts.  
	 * They have less aliasing artifacts than standard shadowmaps, but inherit all the limitations of distance field representations (only uniform scale, no deformation).
	 * These shadows have a low per-object cost (and don't depend on triangle count) so they are effective for distant shadows from a dynamic sun.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldShadows, meta=(DisplayName = "RayTraced DistanceField Shadows"))
	bool bUseRayTracedDistanceFieldShadows;

	/** 
	 * Controls how large of an offset ray traced shadows have from the receiving surface as the camera gets further away.  
	 * This can be useful to hide self-shadowing artifacts from low resolution distance fields on huge static meshes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldShadows, meta=(UIMin = "0", UIMax = ".1"), AdvancedDisplay)
	float RayStartOffsetDepthScale;

	// NvFlow begin
	/** If true, then Flow grid shadow is generated depended of FlowGridShadowChannel match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NvFlow")
	bool bFlowGridShadowEnabled;

	/** If value is the same as ShadowChannel in FlowGridComponent, then this Light is used to generate Flow grid shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NvFlow")
	int32 FlowGridShadowChannel;
	// NvFlow end

	// NVCHANGE_BEGIN: Add VXGI

		/**
	* Whether to let this light cast VXGI indirect lighting and reflections. Only available for Movable lights.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VXGI, meta = (DisplayName = "VXGI Indirect Lighting"))
	bool bCastVxgiIndirectLighting;

	// NVCHANGE_END: Add VXGIpublic:
	/** Set intensity of the light */




	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	/** If enable the nvidia volumetric lighting for this light */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		uint32 bEnableVolumetricLighting : 1;

	/** If true, use the custom volumetric lighting color/intensity, if false, use the light color/intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		uint32 bUseVolumetricLightingColor : 1;

	UPROPERTY(BlueprintReadOnly, interp, Category = NvidiaVolumetricLighting, meta = (UIMin = "0.0", UIMax = "20.0"))
		float VolumetricLightingIntensity;

	UPROPERTY(BlueprintReadOnly, interp, Category = NvidiaVolumetricLighting, meta = (HideAlphaChannel))
		FColor VolumetricLightingColor;

	/** Target minimum ray width in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		float TargetRayResolution;

	/** Amount to bias ray geometry depth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		float DepthBias;

	/** Quality level of tessellation to use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		TEnumAsByte<ETessellationQuality::Type> TessQuality;
	// NVCHANGE_END: Nvidia Volumetric Lighting


	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetIntensity(float NewIntensity);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetIndirectLightingIntensity(float NewIntensity);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetVolumetricScatteringIntensity(float NewIntensity);

	/** Set color of the light */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetLightColor(FLinearColor NewLightColor, bool bSRGB = true);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Light")
	void SetTemperature(float NewTemperature);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetLightFunctionMaterial(UMaterialInterface* NewLightFunctionMaterial);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetLightFunctionScale(FVector NewLightFunctionScale);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetLightFunctionFadeDistance(float NewLightFunctionFadeDistance);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetLightFunctionDisabledBrightness(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetAffectDynamicIndirectLighting(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetAffectTranslucentLighting(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetEnableLightShaftBloom(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetBloomScale(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetBloomThreshold(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetBloomTint(FColor NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light", meta=(DisplayName = "Set IES Texture"))
	void SetIESTexture(UTextureLightProfile* NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetShadowBias(float NewValue);

public:
	/** The light's scene info. */
	class FLightSceneProxy* SceneProxy;

	FStaticShadowDepthMap StaticShadowDepthMap;

	/** Fence used to track progress of render resource destruction. */
	FRenderCommandFence DestroyFence;

	/** true when this light component has been added to the scene as a normal visible light. Used to keep track of whether we need to dirty the render state in UpdateColorAndBrightness */
	uint32 bAddedToSceneVisible:1;

	/**
	 * Test whether this light affects the given primitive.  This checks both the primitive and light settings for light relevance
	 * and also calls AffectsBounds.
	 * @param PrimitiveSceneInfo - The primitive to test.
	 * @return True if the light affects the primitive.
	 */
	bool AffectsPrimitive(const UPrimitiveComponent* Primitive) const;

	/**
	 * Test whether the light affects the given bounding volume.
	 * @param Bounds - The bounding volume to test.
	 * @return True if the light affects the bounding volume
	 */
	virtual bool AffectsBounds(const FBoxSphereBounds& InBounds) const;

	/**
	 * Return the world-space bounding box of the light's influence.
	 */
	virtual FBox GetBoundingBox() const { return FBox(FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX),FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX)); }

	virtual FSphere GetBoundingSphere() const
	{
		return FSphere(FVector::ZeroVector, HALF_WORLD_MAX);
	}

	/**
	 * Return the homogenous position of the light.
	 */
	virtual FVector4 GetLightPosition() const PURE_VIRTUAL(ULightComponent::GetPosition,return FVector4(););

	/**
	* @return ELightComponentType for the light component class
	*/
	virtual ELightComponentType GetLightType() const PURE_VIRTUAL(ULightComponent::GetLightType,return LightType_MAX;);

	virtual FLightmassLightSettings GetLightmassSettings() const PURE_VIRTUAL(ULightComponent::GetLightmassSettings,return FLightmassLightSettings(););

	virtual float GetUniformPenumbraSize() const PURE_VIRTUAL(ULightComponent::GetUniformPenumbraSize,return 0;);


	/**
	 * Check whether a given primitive will cast shadows from this light.
	 * @param Primitive - The potential shadow caster.
	 * @return Returns True if a primitive blocks this light.
	 */
	bool IsShadowCast(UPrimitiveComponent* Primitive) const;

	/* Whether to consider light as a sunlight for atmospheric scattering. */  
	virtual bool IsUsedAsAtmosphereSunLight() const
	{
		return false;
	}

	/** Compute current light brightness based on whether there is a valid IES profile texture attached, and whether IES brightness is enabled */
	float ComputeLightBrightness() const;

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void UpdateLightSpriteTexture() override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	//~ End UObject Interface.

	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	void ApplyComponentInstanceData(class FPrecomputedLightInstanceData* ComponentInstanceData);
	virtual void PropagateLightingScenarioChange() override;
	virtual bool IsPrecomputedLightingValid() const override;

	/** @return number of material elements in this primitive */
	virtual int32 GetNumMaterials() const;

	/** @return MaterialInterface assigned to the given material index (if any) */
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;

	/** Set the MaterialInterface to use for the given element index (if valid) */
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial);

	virtual class FLightSceneProxy* CreateSceneProxy() const
	{
		return NULL;
	}

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	virtual void GetNvVlAttenuation(int32& OutAttenuationMode, FVector4& OutAttenuationFactors) const
	{
		OutAttenuationMode = 0;
		OutAttenuationFactors = FVector4(0);
	}

	virtual void GetNvVlFalloff(int32& OutFalloffMode, FVector2D& OutFalloffAngleAndPower) const
	{
		OutFalloffMode = 0;
		OutFalloffAngleAndPower = FVector2D::ZeroVector;
	}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting



protected:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ Begin UActorComponent Interface

public:
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;

	/** Invalidates the light's cached lighting with the option to recreate the light Guids. */
	void InvalidateLightingCacheInner(bool bRecreateLightGuids);

	/** Script interface to retrieve light direction. */
	FVector GetDirection() const;

	/** Script interface to update the color and brightness on the render thread. */
	void UpdateColorAndBrightness();

	const FLightComponentMapBuildData* GetLightComponentMapBuildData() const;

	void InitializeStaticShadowDepthMap();

	/** 
	 * Called when property is modified by InterpPropertyTracks
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostInterpChange(UProperty* PropertyThatChanged) override;

	/** 
	 * Iterates over ALL stationary light components in the target world and assigns their preview shadowmap channel, and updates light icons accordingly.
	 * Also handles assignment after a lighting build, so that the same algorithm is used for previewing and static lighting.
	 */
	static void ReassignStationaryLightChannels(UWorld* TargetWorld, bool bAssignForLightingBuild, ULevel* LightingScenario);

};



