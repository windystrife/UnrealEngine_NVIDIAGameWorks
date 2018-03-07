// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneTypes.h"
#include "Math/SHMath.h"

class AWorldSettings;
class FAtmosphericFogSceneInfo;
class FMaterial;
class FMaterialShaderMap;
class FPrimitiveSceneInfo;
class FRenderResource;
class FRenderTarget;
class FSkyLightSceneProxy;
class FTexture;
class FVertexFactory;
class UDecalComponent;
class ULightComponent;
class UPlanarReflectionComponent;
class UPrimitiveComponent;
class UReflectionCaptureComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UTextureCube;

enum EBasePassDrawListType
{
	EBasePass_Default=0,
	EBasePass_Masked,
	EBasePass_MAX
};

enum class EShadingPath
{
	Mobile,
	Deferred,
	Num,
};

/**
 * An interface to the private scene manager implementation of a scene.  Use GetRendererModule().AllocateScene to create.
 * The scene
 */
class FSceneInterface
{
public:

	// FSceneInterface interface

	/** 
	 * Adds a new primitive component to the scene
	 * 
	 * @param Primitive - primitive component to add
	 */
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) = 0;
	/** 
	 * Removes a primitive component from the scene
	 * 
	 * @param Primitive - primitive component to remove
	 */
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) = 0;
	/** Called when a primitive is being unregistered and will not be immediately re-registered. */
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) = 0;
	/** 
	 * Updates the transform of a primitive which has already been added to the scene. 
	 * 
	 * @param Primitive - primitive component to update
	 */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) = 0;
	/** Updates primitive attachment state. */
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) = 0;
	/** Finds the  primitive with the associated component id. */
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimitiveIndex) = 0;
	/** 
	 * Adds a new light component to the scene
	 * 
	 * @param Light - light component to add
	 */
	virtual void AddLight(ULightComponent* Light) = 0;
	/** 
	 * Removes a light component from the scene
	 * 
	 * @param Light - light component to remove
	 */
	virtual void RemoveLight(ULightComponent* Light) = 0;
	/** 
	 * Adds a new light component to the scene which is currently invisible, but needed for editor previewing
	 * 
	 * @param Light - light component to add
	 */
	virtual void AddInvisibleLight(ULightComponent* Light) = 0;
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) = 0;
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) = 0;
	/** 
	 * Adds a new decal component to the scene
	 * 
	 * @param Component - component to add
	 */
	virtual void AddDecal(UDecalComponent* Component) = 0;
	/** 
	 * Removes a decal component from the scene
	 * 
	 * @param Component - component to remove
	 */
	virtual void RemoveDecal(UDecalComponent* Component) = 0;
	/** 
	 * Updates the transform of a decal which has already been added to the scene. 
	 *
	 * @param Decal - Decal component to update
	 */
	virtual void UpdateDecalTransform(UDecalComponent* Component) = 0;

	/** Adds a reflection capture to the scene. */
	virtual void AddReflectionCapture(class UReflectionCaptureComponent* Component) {}

	/** Removes a reflection capture from the scene. */
	virtual void RemoveReflectionCapture(class UReflectionCaptureComponent* Component) {}

	/** Reads back reflection capture data from the GPU.  Very slow operation that blocks the GPU and rendering thread many times. */
	virtual void GetReflectionCaptureData(UReflectionCaptureComponent* Component, class FReflectionCaptureFullHDR& OutDerivedData) {}

	/** Updates a reflection capture's transform, and then re-captures the scene. */
	virtual void UpdateReflectionCaptureTransform(class UReflectionCaptureComponent* Component) {}

	/** 
	 * Allocates reflection captures in the scene's reflection cubemap array and updates them by recapturing the scene.
	 * Existing captures will only be updated.  Must be called from the game thread.
	 */
	virtual void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures) {}
	virtual void ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent) {}

	/** 
	 * Updates the contents of the given sky capture by rendering the scene. 
	 * This must be called on the game thread.
	 */
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, UTextureCube* SourceCubemap, FTexture* OutProcessedTexture, float& OutAverageBrightness, FSHVectorRGB3& OutIrradianceEnvironmentMap, TArray<FFloat16Color>* OutRadianceMap) {}

	virtual void AddPlanarReflection(class UPlanarReflectionComponent* Component) {}
	virtual void RemovePlanarReflection(class UPlanarReflectionComponent* Component) {}
	virtual void UpdatePlanarReflectionTransform(UPlanarReflectionComponent* Component) {}

	/** 
	* Updates the contents of the given scene capture by rendering the scene. 
	* This must be called on the game thread.
	*/
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponent2D* CaptureComponent) {}
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponentCube* CaptureComponent) {}
// WaveWorks Start
	virtual void UpdateSceneCaptureContents(class UWaveWorksShorelineCaptureComponent* CaptureComponent) {}
// WaveWorks End
	virtual void UpdatePlanarReflectionContents(class UPlanarReflectionComponent* CaptureComponent, class FSceneRenderer& MainSceneRenderer) {}

	virtual void AddPrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) {}
	virtual void RemovePrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) {}

	virtual bool HasPrecomputedVolumetricLightmap_RenderThread() const { return false; }
	virtual void AddPrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) {}
	virtual void RemovePrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) {}

	/** 
	 * Updates the transform of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightTransform(ULightComponent* Light) = 0;
	/** 
	 * Updates the color and brightness of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) = 0;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const class FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler) {}

	/** Sets the precomputed volume distance field for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVolumeDistanceField(const class FPrecomputedVolumeDistanceField* PrecomputedVolumeDistanceField) {}

	/** Sets shader maps on the specified materials without blocking. */
	virtual void SetShaderMapsOnMaterialResources(const TMap<FMaterial*, FMaterialShaderMap*>& MaterialsToUpdate) {}

	/** Updates static draw lists for the given set of materials. */
	virtual void UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials) {}

	/** 
	 * Adds a new exponential height fog component to the scene
	 * 
	 * @param FogComponent - fog component to add
	 */	
	virtual void AddExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;
	/** 
	 * Removes a exponential height fog component from the scene
	 * 
	 * @param FogComponent - fog component to remove
	 */	
	virtual void RemoveExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;

	/** 
	 * Adds a new atmospheric fog component to the scene
	 * 
	 * @param FogComponent - fog component to add
	 */	
	virtual void AddAtmosphericFog(class UAtmosphericFogComponent* FogComponent) = 0;

	/** 
	 * Removes a atmospheric fog component from the scene
	 * 
	 * @param FogComponent - fog component to remove
	 */	
	virtual void RemoveAtmosphericFog(class UAtmosphericFogComponent* FogComponent) = 0;

	/** 
	 * Removes a atmospheric fog resource from the scene...this is just a double check to make sure we don't have stale stuff hanging around; should already be gone.
	 * 
	 * @param FogResource - fog resource to remove
	 */	
	virtual void RemoveAtmosphericFogResource_RenderThread(FRenderResource* FogResource) = 0;

	/**
	 * Returns the scene's FAtmosphericFogSceneInfo if it exists
	 */
	virtual FAtmosphericFogSceneInfo* GetAtmosphericFogSceneInfo() = 0;

	/**
	 * Adds a wind source component to the scene.
	 * @param WindComponent - The component to add.
	 */
	virtual void AddWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Removes a wind source component from the scene.
	 * @param WindComponent - The component to remove.
	 */
	virtual void RemoveWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Accesses the wind source list.  Must be called in the rendering thread.
	 * @return The list of wind sources in the scene.
	 */
	virtual const TArray<class FWindSourceSceneProxy*>& GetWindSources_RenderThread() const = 0;

	/** Accesses wind parameters.  XYZ will contain wind direction * Strength, W contains wind speed. */
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** Accesses wind parameters safely for game thread applications */
	virtual void GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** Same as GetWindParameters, but ignores point wind sources. */
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** 
	 * Adds a SpeedTree wind computation object to the scene.
	 * @param StaticMesh - The SpeedTree static mesh whose wind to add.
	 */
	virtual void AddSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) = 0;

	/** 
	 * Removes a SpeedTree wind computation object to the scene.
	 * @param StaticMesh - The SpeedTree static mesh whose wind to remove.
	 */
	virtual void RemoveSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) = 0;
	virtual void RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) = 0;

	/** Ticks the SpeedTree wind object and updates the uniform buffer. */
	virtual void UpdateSpeedTreeWind(double CurrentTime) = 0;

	/** 
	 * Looks up the SpeedTree uniform buffer for the passed in vertex factory.
	 * @param VertexFactory - The vertex factory registered for SpeedTree.
	 */
	virtual FUniformBufferRHIParamRef GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) = 0;

	/**
	 * Release this scene and remove it from the rendering thread
	 */
	virtual void Release() = 0;
	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const = 0;
	/**
	 * Indicates if hit proxies should be processed by this scene
	 *
	 * @return true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const = 0;
	/**
	 * Get the optional UWorld that is associated with this scene
	 * 
	 * @return UWorld instance used by this scene
	 */
	virtual class UWorld* GetWorld() const = 0;
	/**
	 * Return the scene to be used for rendering. Note that this can return NULL if rendering has
	 * been disabled!
	 */
	virtual class FScene* GetRenderScene()
	{
		return NULL;
	}

	virtual void UpdateSceneSettings(AWorldSettings* WorldSettings) {}


	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	virtual void UpdateVolumetricLightingSettings(AWorldSettings* WorldSettings) {}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	/**
	* Gets the GPU Skin Cache system associated with the scene.
	*/
	virtual class FGPUSkinCache* GetGPUSkinCache()
	{
		return nullptr;
	}

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) = 0;

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() = 0;

	virtual void DumpUnbuiltLightInteractions( FOutputDevice& Ar ) const { }

	/**
	 * Dumps static mesh draw list stats to the log.
	 */
	virtual void DumpStaticMeshDrawListStats() const {}

	/**
	 * Request to clear the MB info. Game thread only
	 */
	virtual void SetClearMotionBlurInfoGameThread() {}

	/** Updates the scene's list of parameter collection id's and their uniform buffers. */
	virtual void UpdateParameterCollections(const TArray<class FMaterialParameterCollectionInstanceResource*>& InParameterCollections) {}

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const
	{}

	
	/**
	 * Shifts scene data by provided delta
	 * Called on world origin changes
	 * 
	 * @param	InOffset	Delta to shift scene by
	 */
	virtual void ApplyWorldOffset(FVector InOffset) {}

	/**
	 * Notification that level was added to a world
	 * 
	 * @param	InLevelName		Level name
	 */
	virtual void OnLevelAddedToWorld(FName InLevelName, UWorld* InWorld, bool bIsLightingScenario) {}
	virtual void OnLevelRemovedFromWorld(UWorld* InWorld, bool bIsLightingScenario) {}

	/**
	 * @return True if there are any lights in the scene
	 */
	virtual bool HasAnyLights() const = 0;

	virtual bool IsEditorScene() const { return false; }

	virtual ERHIFeatureLevel::Type GetFeatureLevel() const { return GMaxRHIFeatureLevel; }
	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[GetFeatureLevel()]; }

	static EShadingPath GetShadingPath(ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (InFeatureLevel >= ERHIFeatureLevel::SM4)
		{
			return EShadingPath::Deferred;
		}
		else
		{
			return EShadingPath::Mobile;
		}
	}

	EShadingPath GetShadingPath() const
	{
		return GetShadingPath(GetFeatureLevel());
	}

#if WITH_EDITOR
	/**
	 * Initialize the pixel inspector buffers.
	 * @return True if implemented false otherwise.
	 */
	virtual bool InitializePixelInspector(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDE, int32 BufferIndex)
	{
		return false;
	}

	/**
	 * Add a pixel inspector request.
	 * @return True if implemented false otherwise.
	 */
	virtual bool AddPixelInspectorRequest(class FPixelInspectorRequest *PixelInspectorRequest)
	{
		return false;
	}
#endif //WITH_EDITOR

	/**
	 * Returns the FPrimitiveComponentId for all primitives in the scene
	 */
	virtual ENGINE_API TArray<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const;

	virtual uint32 GetFrameNumber() const { return 0; }
	virtual void IncrementFrameNumber() {}

protected:
	virtual ~FSceneInterface() {}
};
