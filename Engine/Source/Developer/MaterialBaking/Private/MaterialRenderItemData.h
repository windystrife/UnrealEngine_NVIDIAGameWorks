// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LightMapHelpers.h"
#include "Components.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"

/**
* Vertex data for a screen quad.
*/
struct FMaterialMeshVertex
{
	FVector			Position;
	FPackedNormal	TangentX, TangentZ;
	uint32			Color;
	FVector2D		TextureCoordinate[MAX_STATIC_TEXCOORDS];
	FVector2D		LightMapCoordinate;

	void SetTangents(const FVector& InTangentX, const FVector& InTangentY, const FVector& InTangentZ)
	{
		TangentX = InTangentX;
		TangentZ = InTangentZ;
		// store determinant of basis in w component of normal vector
		TangentZ.Vector.W = GetBasisDeterminantSign(InTangentX, InTangentY, InTangentZ) < 0.0f ? 0 : 255;
	}
};

/**
* A dummy vertex buffer used to give the FMeshVertexFactory something to reference as a stream source.
*/
class FMaterialMeshVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMaterialMeshVertex), BUF_Static, CreateInfo);
	}

	static TGlobalResource<FMaterialMeshVertexBuffer> DummyMeshRendererVertexBuffer;
};

/**
* Vertex factory for rendering meshes with materials.
*/
class FMeshVertexFactory : public FLocalVertexFactory
{
public:

	/** Default constructor. */
	FMeshVertexFactory();


	static TGlobalResource<FMeshVertexFactory> MeshVertexFactory;
};

/** Simple implementation for light cache to simulated lightmap behaviour (used for accessing prebaked ambient occlusion values) */
class FMeshRenderInfo : public FLightCacheInterface
{
public:
	FMeshRenderInfo(const FLightMap* InLightMap, const FShadowMap* InShadowMap, FUniformBufferRHIRef Buffer)
		: FLightCacheInterface(InLightMap, InShadowMap)
	{
		SetPrecomputedLightingBuffer(Buffer);
	}

	virtual FLightInteraction GetInteraction(const class FLightSceneProxy* LightSceneProxy) const override
	{
		return LIT_CachedLightMap;
	}
};