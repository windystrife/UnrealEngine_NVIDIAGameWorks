// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BatchedElements.h"
#include "MeshPaintRendering.h"

class UMeshComponent;
class UTexture;
class UTexture2D;
class UTextureRenderTarget2D;

/** Batched element parameters for texture paint shaders used for paint blending and paint mask generation */
class MESHPAINT_API FMeshPaintBatchedElementParameters : public FBatchedElementParameters
{
public:
	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override
	{
		MeshPaintRendering::SetMeshPaintShaders(RHICmdList, GraphicsPSOInit, InFeatureLevel, InTransform, InGamma, ShaderParams);
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintShaderParameters ShaderParams;
};

/** Batched element parameters for texture paint shaders used for texture dilation */
class MESHPAINT_API FMeshPaintDilateBatchedElementParameters : public FBatchedElementParameters
{
public:
	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override
	{
		MeshPaintRendering::SetMeshPaintDilateShaders(RHICmdList, GraphicsPSOInit, InFeatureLevel, InTransform, InGamma, ShaderParams);
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintDilateShaderParameters ShaderParams;
};

/** Helper struct to store mesh section information in*/
struct FTexturePaintMeshSectionInfo
{
	/** First vertex index in section */
	int32 FirstIndex;

	/** Last vertex index in section */
	int32 LastIndex;
};

class IMeshPaintGeometryAdapter;
struct FPaintableTexture;

/** Helpers functions for texture painting functionality */
class MESHPAINT_API TexturePaintHelpers
{
public:
	/** Static: Copies a texture to a render target texture */
	static void CopyTextureToRenderTargetTexture(UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture, ERHIFeatureLevel::Type FeatureLevel);

	/** Will generate a mask texture, used for texture dilation, and store it in the passed in render target */
	static bool GenerateSeamMask(UMeshComponent* MeshComponent, int32 UVSet, UTextureRenderTarget2D* SeamRenderTexture, UTexture2D* Texture, UTextureRenderTarget2D* RenderTargetTexture );

	/** Static: Creates a temporary texture used to transfer data to a render target in memory */
	static UTexture2D* CreateTempUncompressedTexture(UTexture2D* SourceTexture);

	/** Makes sure that the render target is ready to paint on */
	static void SetupInitialRenderTargetData(UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget);

	/** Tries to find Materials using the given Texture and retrieve the corresponding material indices from the MEsh Compon*/
	static void FindMaterialIndicesUsingTexture(const UTexture* Texture, const UMeshComponent* MeshComponent, TArray<int32>& OutIndices);
	
	/** Retrieve LOD mesh sections from MeshComponent which use one of the given textures */
	static void RetrieveMeshSectionsForTextures(const UMeshComponent* MeshComponent, int32 LODIndex, TArray<const UTexture*> Textures, TArray<FTexturePaintMeshSectionInfo>& OutSectionInfo);

	/** Retrieve LOD mesh sections from MeshComponent which contain one of the Material Indices */
	static void RetrieveMeshSectionsForMaterialIndices(const UMeshComponent* MeshComponent, int32 LODIndex, const TArray<int32>& MaterialIndices, TArray<FTexturePaintMeshSectionInfo>& OutSectionInfo);

	/** Retrieves all Paintable Textures from the given MeshComponent */
	static void RetrieveTexturesForComponent(const UMeshComponent* Component, IMeshPaintGeometryAdapter* Adapter, TArray<FPaintableTexture>& OutTextures);

	/** Checks whether or not the given MeshComponent's materials use the specified texture */
	static bool DoesMeshComponentUseTexture(UMeshComponent* MeshComponent, UTexture* Texture);
};