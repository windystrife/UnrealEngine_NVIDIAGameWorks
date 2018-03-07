// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateShaderResource.h"
#include "Materials/MaterialInterface.h"

class FMaterialRenderProxy;

/**
 * A resource for rendering a UMaterial in Slate
 */
class FSlateMaterialResource : public FSlateShaderResource
{
public:
	FSlateMaterialResource(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask = nullptr );
	~FSlateMaterialResource();

	virtual uint32 GetWidth() const override { return Width; }
	virtual uint32 GetHeight() const override { return Height; }
	virtual ESlateShaderResource::Type GetType() const override { return ESlateShaderResource::Material; }

	void UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2D& InImageSize, FSlateShaderResource* InTextureMask );
	void ResetMaterial();

	/** @return The material render proxy */
	FMaterialRenderProxy* GetRenderProxy() const { return MaterialObject->GetRenderProxy(false, false); }

	/** @return the material object */
	const UMaterialInterface* GetMaterialObject() const { return MaterialObject; }

	FSlateShaderResource* GetTextureMaskResource() const { return TextureMaskResource; }
public:
	const class UMaterialInterface* MaterialObject;
#if !UE_BUILD_SHIPPING
	// Used to guard against crashes when the material object is deleted.  This is expensive so we do not do it in shipping
	TWeakObjectPtr<UMaterialInterface> MaterialObjectWeakPtr;
	FName MaterialName;
#endif
	/** Slate proxy used for batching the material */
	FSlateShaderResourceProxy* SlateProxy;

	FSlateShaderResource* TextureMaskResource;
	uint32 Width;
	uint32 Height;

private:
#if !UE_BUILD_SHIPPING
	void UpdateMaterialName();
#endif
};

