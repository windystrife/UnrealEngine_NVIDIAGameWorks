// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "PixelFormat.h"
#include "SceneTypes.h"

class UTextureRenderTarget2D;
class UMaterialOptions;
class UMaterialInterface;

class FExportMaterialProxy;
class FTextureRenderTargetResource;

struct FMaterialData; 
struct FMeshData;
struct FBakeOutput;

class FMaterialBakingModule : public IModuleInterface
{
public:
	/** IModuleInterface overrides begin */
	virtual void StartupModule();
	virtual void ShutdownModule();
	/** IModuleInterface overrides end */

	/** Bakes out material properties according to MaterialSettings using MeshSettings and stores the output in Output */
	MATERIALBAKING_API static void BakeMaterials(const TArray<FMaterialData*>& MaterialSettings, const TArray<FMeshData*>& MeshSettings, TArray<FBakeOutput>& Output);

	/** Promps a slate window to allow the user to populate specific material baking settings used while baking out materials */
	MATERIALBAKING_API static bool SetupMaterialBakeSettings(TArray<TWeakObjectPtr<UObject>>& OptionObjects, int32 NumLODs);

protected:
	/* Creates and adds or reuses a RenderTarget from the pool */
	static UTextureRenderTarget2D* CreateRenderTarget(bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint& InTargetSize);

	/* Creates and adds (or reuses a ExportMaterialProxy from the pool if MaterialBaking.UseMaterialProxyCaching is set to 1) */
	static FExportMaterialProxy* CreateMaterialProxy(UMaterialInterface* Material, const EMaterialProperty Property );
	
	/** Helper function to read pixel data from the given render target to Output */
	static void ReadTextureOutput(FTextureRenderTargetResource* RenderTargetResource, EMaterialProperty Property, FBakeOutput& Output);	
	
	/** Cleans up all cached material proxies in MaterialProxyPool */
	static void CleanupMaterialProxies();

	/** Callback for modified objects which should be removed from MaterialProxyPool in that case */
	void OnObjectModified(UObject* Object);
private:
	/** Pool of available render targets, cached for re-using on consecutive property rendering */
	static TArray<UTextureRenderTarget2D*> RenderTargetPool;

	/** Pool of cached material proxies to optimize material baking workflow, stays resident when MaterialBaking.UseMaterialProxyCaching is set to 1 */
	static TMap<TPair<UMaterialInterface*, EMaterialProperty>, FExportMaterialProxy*> MaterialProxyPool;

	/** Pixel formats to use for baking out specific material properties */
	static EPixelFormat PerPropertyFormat[MP_MAX];

	/** Whether or not to enforce gamma correction while baking out specific material properties */
	static bool PerPropertyGamma[MP_MAX];
};
