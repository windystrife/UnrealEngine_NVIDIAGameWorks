// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExternalTexture.h"


#define EXTERNALTEXTURE_TRACE_REGISTRY 0


FExternalTextureRegistry* FExternalTextureRegistry::Singleton = nullptr;


FExternalTextureRegistry& FExternalTextureRegistry::Get()
{
	check(IsInRenderingThread());

	if (Singleton == nullptr)
	{
		Singleton = new FExternalTextureRegistry();
	}

	return *Singleton;
}


void FExternalTextureRegistry::RegisterExternalTexture(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI, const FLinearColor& InCoordinateScaleRotation, const FLinearColor& InCoordinateOffset)
{
	TextureEntries.Add(InGuid, FExternalTextureEntry(InTextureRHI, InSamplerStateRHI, InCoordinateScaleRotation, InCoordinateOffset));

	for (const FMaterialRenderProxy* MaterialRenderProxy : ReferencingMaterialRenderProxies)
	{
		const_cast<FMaterialRenderProxy*>(MaterialRenderProxy)->CacheUniformExpressions();
	}
}


void FExternalTextureRegistry::UnregisterExternalTexture(const FGuid& InGuid)
{
	TextureEntries.Remove(InGuid);

	for (const FMaterialRenderProxy* MaterialRenderProxy : ReferencingMaterialRenderProxies)
	{
		const_cast<FMaterialRenderProxy*>(MaterialRenderProxy)->CacheUniformExpressions();
	}
}


void FExternalTextureRegistry::RemoveMaterialRenderProxyReference(const FMaterialRenderProxy* MaterialRenderProxy)
{
	ReferencingMaterialRenderProxies.Remove(MaterialRenderProxy);
}


bool FExternalTextureRegistry::GetExternalTexture(const FMaterialRenderProxy* MaterialRenderProxy, const FGuid& InGuid, FTextureRHIRef& OutTextureRHI, FSamplerStateRHIRef& OutSamplerStateRHI)
{
#if EXTERNALTEXTURE_TRACE_REGISTRY
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GetExternalTexture: Guid = %s"), *InGuid.ToString());
#endif

	// register material proxy if already initialized
	if ((MaterialRenderProxy != nullptr) && MaterialRenderProxy->IsInitialized())
	{
		ReferencingMaterialRenderProxies.Add(MaterialRenderProxy);

		// Note: FMaterialRenderProxy::ReleaseDynamicRHI()
		// is responsible for removing the material proxy
	}

	if (!InGuid.IsValid())
	{
		return false; // no identifier associated with the texture yet
	}

	FExternalTextureEntry* Entry = TextureEntries.Find(InGuid);

	if (Entry == nullptr)
	{
#if EXTERNALTEXTURE_TRACE_REGISTRY
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GetExternalTexture: NOT FOUND!"));
#endif

		return false; // texture not registered
	}

#if EXTERNALTEXTURE_TRACE_REGISTRY
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GetExternalTexture: Found"));
#endif

	OutTextureRHI = Entry->TextureRHI;
	OutSamplerStateRHI = Entry->SamplerStateRHI;

	return true;
}


bool FExternalTextureRegistry::GetExternalTextureCoordinateScaleRotation(const FGuid& InGuid, FLinearColor& OutCoordinateScaleRotation)
{
	FExternalTextureEntry* Entry = TextureEntries.Find(InGuid);

	if (Entry == nullptr)
	{
		return false;
	}

	OutCoordinateScaleRotation = Entry->CoordinateScaleRotation;

	return true;
}


bool FExternalTextureRegistry::GetExternalTextureCoordinateOffset(const FGuid& InGuid, FLinearColor& OutCoordinateOffset)
{
	FExternalTextureEntry* Entry = TextureEntries.Find(InGuid);

	if (Entry == nullptr)
	{
		return false;
	}

	OutCoordinateOffset = Entry->CoordinateOffset;

	return true;
}
