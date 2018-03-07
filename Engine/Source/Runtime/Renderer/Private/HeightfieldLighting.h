// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HeightfieldLighting.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Engine/Texture2D.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"
#include "PrimitiveSceneProxy.h"

class FAOScreenGridResources;
class FDistanceFieldAOParameters;
class FLightSceneInfo;
class FLightTileIntersectionResources;
class FProjectedShadowInfo;
class FScene;
class FViewInfo;
struct Rect;

class FHeightfieldLightingAtlas : public FRenderResource
{
public:

	TRefCountPtr<IPooledRenderTarget> Height;
	TRefCountPtr<IPooledRenderTarget> Normal;
	TRefCountPtr<IPooledRenderTarget> DiffuseColor;
	TRefCountPtr<IPooledRenderTarget> DirectionalLightShadowing;
	TRefCountPtr<IPooledRenderTarget> Lighting;

	FHeightfieldLightingAtlas() :
		AtlasSize(FIntPoint(0, 0))
	{}

	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	void InitializeForSize(FIntPoint InAtlasSize);

	FIntPoint GetAtlasSize() const { return AtlasSize; }

private:

	FIntPoint AtlasSize;
};

class FHeightfieldComponentTextures
{
public:

	FHeightfieldComponentTextures(UTexture2D* InHeightAndNormal, UTexture2D* InDiffuseColor) :
		HeightAndNormal(InHeightAndNormal),
		DiffuseColor(InDiffuseColor)
	{}

	FORCEINLINE bool operator==(FHeightfieldComponentTextures Other) const
	{
		return HeightAndNormal == Other.HeightAndNormal && DiffuseColor == Other.DiffuseColor;
	}

	FORCEINLINE friend uint32 GetTypeHash(FHeightfieldComponentTextures ComponentTextures)
	{
		return GetTypeHash(ComponentTextures.HeightAndNormal);
	}

	UTexture2D* HeightAndNormal;
	UTexture2D* DiffuseColor;
};

class FHeightfieldDescription
{
public:
	FIntRect Rect;
	int32 DownsampleFactor;
	FIntRect DownsampledRect;

	TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>> ComponentDescriptions;

	FHeightfieldDescription() :
		Rect(FIntRect(0, 0, 0, 0)),
		DownsampleFactor(1),
		DownsampledRect(FIntRect(0, 0, 0, 0))
	{}
};

class FHeightfieldLightingViewInfo
{
public:

	FHeightfieldLightingViewInfo()
	{}

	void SetupVisibleHeightfields(const FViewInfo& View, FRHICommandListImmediate& RHICmdList);

	void SetupHeightfieldsForScene(const FScene& Scene, FRHICommandListImmediate& RHICmdList);

	void ClearShadowing(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo) const;

	void ComputeShadowMapShadowing(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FProjectedShadowInfo* ProjectedShadowInfo) const;

	void ComputeRayTracedShadowing(
		const FViewInfo& View, 
		FRHICommandListImmediate& RHICmdList, 
		const FProjectedShadowInfo* ProjectedShadowInfo, 
		FLightTileIntersectionResources* TileIntersectionResources,
		class FDistanceFieldObjectBufferResource& CulledObjectBuffers) const;

	void ComputeLighting(const FViewInfo& View, FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo) const;

	void ComputeOcclusionForScreenGrid(
		const FViewInfo& View, 
		FRHICommandListImmediate& RHICmdList, 
		FSceneRenderTargetItem& DistanceFieldNormal,
		const class FAOScreenGridResources& ScreenGridResources,
		const class FDistanceFieldAOParameters& Parameters) const;

	void ComputeIrradianceForScreenGrid(
		const FViewInfo& View, 
		FRHICommandListImmediate& RHICmdList, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FAOScreenGridResources& ScreenGridResources,
		const FDistanceFieldAOParameters& Parameters) const;

	void CompositeHeightfieldsIntoGlobalDistanceField(
		FRHICommandList& RHICmdList,
		const FScene* Scene,
		const FViewInfo& View,
		float MaxOcclusionDistance,
		const class FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo,
		int32 ClipmapIndexValue,
		const class FVolumeUpdateRegion& UpdateRegion) const;

private:

	FHeightfieldDescription Heightfield;
};

extern FShaderResourceViewRHIParamRef GetHeightfieldDescriptionsSRV();

class FHeightfieldDescriptionParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		HeightfieldDescriptions.Bind(ParameterMap, TEXT("HeightfieldDescriptions"));
		NumHeightfields.Bind(ParameterMap, TEXT("NumHeightfields"));
	}

	friend FArchive& operator<<(FArchive& Ar, FHeightfieldDescriptionParameters& Parameters)
	{
		Ar << Parameters.HeightfieldDescriptions;
		Ar << Parameters.NumHeightfields;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, FShaderResourceViewRHIParamRef HeightfieldDescriptionsValue, int32 NumHeightfieldsValue)
	{
		SetSRVParameter(RHICmdList, ShaderRHI, HeightfieldDescriptions, HeightfieldDescriptionsValue);
		SetShaderValue(RHICmdList, ShaderRHI, NumHeightfields, NumHeightfieldsValue);
	}

private:
	FShaderResourceParameter HeightfieldDescriptions;
	FShaderParameter NumHeightfields;
};

class FHeightfieldTextureParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		HeightfieldTexture.Bind(ParameterMap, TEXT("HeightfieldTexture"));
		HeightfieldSampler.Bind(ParameterMap, TEXT("HeightfieldSampler"));
		DiffuseColorTexture.Bind(ParameterMap, TEXT("DiffuseColorTexture"));
		DiffuseColorSampler.Bind(ParameterMap, TEXT("DiffuseColorSampler"));
	}

	friend FArchive& operator<<(FArchive& Ar, FHeightfieldTextureParameters& Parameters)
	{
		Ar << Parameters.HeightfieldTexture;
		Ar << Parameters.HeightfieldSampler;
		Ar << Parameters.DiffuseColorTexture;
		Ar << Parameters.DiffuseColorSampler;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, UTexture2D* HeightfieldTextureValue, UTexture2D* DiffuseColorTextureValue)
	{
		//@todo - shouldn't filter the heightfield, it's packed
		SetTextureParameter(RHICmdList, ShaderRHI, HeightfieldTexture, HeightfieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), HeightfieldTextureValue->Resource->TextureRHI);

		if (DiffuseColorTextureValue)
		{
			SetTextureParameter(RHICmdList, ShaderRHI, DiffuseColorTexture, DiffuseColorSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DiffuseColorTextureValue->Resource->TextureRHI);
		}
		else
		{
			SetTextureParameter(RHICmdList, ShaderRHI, DiffuseColorTexture, DiffuseColorSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), GBlackTexture->TextureRHI);
		}
	}

private:
	FShaderResourceParameter HeightfieldTexture;
	FShaderResourceParameter HeightfieldSampler;
	FShaderResourceParameter DiffuseColorTexture;
	FShaderResourceParameter DiffuseColorSampler;
};
