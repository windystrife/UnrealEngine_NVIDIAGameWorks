// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "MaterialShared.h"

/* FExternalTextureRegistry - stores a registry of external textures mapped to their GUIDs */

class ENGINE_API FExternalTextureRegistry
{
	static FExternalTextureRegistry* Singleton;

	FExternalTextureRegistry()
	{}

	struct FExternalTextureEntry
	{
		FExternalTextureEntry(FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI, const FLinearColor& InCoordinateScaleRotation, const FLinearColor& InCoordinateOffset)
			: TextureRHI(InTextureRHI)
			, SamplerStateRHI(InSamplerStateRHI)
			, CoordinateScaleRotation(InCoordinateScaleRotation)
			, CoordinateOffset(InCoordinateOffset)
		{}

		const FTextureRHIRef TextureRHI;
		const FSamplerStateRHIRef SamplerStateRHI;
		FLinearColor CoordinateScaleRotation;
		FLinearColor CoordinateOffset;
	};

	TMap<FGuid, FExternalTextureEntry> TextureEntries;
	TSet<const FMaterialRenderProxy*> ReferencingMaterialRenderProxies;

public:

	static FExternalTextureRegistry& Get();

	/* Register an external texture, its sampler state and coordinate scale/bias against a GUID */
	void RegisterExternalTexture(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI, const FLinearColor& InCoordinateScaleRotation = FLinearColor(1, 0, 0, 1), const FLinearColor& InCoordinateOffset = FLinearColor(0, 0, 0, 0));

	/* Removes an external texture given a GUID */
	void UnregisterExternalTexture(const FGuid& InGuid);

	void RemoveMaterialRenderProxyReference(const FMaterialRenderProxy* MaterialRenderProxy);

	/* Looks up an external texture for given a given GUID
	 * @return false if the texture is not registered
	 */
	bool GetExternalTexture(const FMaterialRenderProxy* MaterialRenderProxy, const FGuid& InGuid, FTextureRHIRef& OutTextureRHI, FSamplerStateRHIRef& OutSamplerStateRHI);

	/* Looks up an texture coordinate scale rotation for given a given GUID
	 * @return false if the texture is not registered
	 */
	bool GetExternalTextureCoordinateScaleRotation(const FGuid& InGuid, FLinearColor& OutCoordinateScaleRotation);

	/* Looks up an texture coordinate offset for given a given GUID
	 * @return false if the texture is not registered
	 */
	bool GetExternalTextureCoordinateOffset(const FGuid& InGuid, FLinearColor& OutCoordinateOffset);
};
