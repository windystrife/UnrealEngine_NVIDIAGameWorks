/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

/**
* WaveWorks Render Classes
*/

#pragma once

#include "UniformBuffer.h"
#include "LocalVertexFactory.h"
#include "PrimitiveSceneProxy.h"

/**
* Uniform buffer for Shoreline properties
*/
BEGIN_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineUniformParameters, ENGINE_API)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,	bUseShoreline)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		GerstnerSteepness)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		GerstnerAmplitude)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		GerstnerWavelength)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		GerstnerSpeed)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		GerstnerParallelity)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int,		GerstnerWaves)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int,		MaxPixelsToShoreline)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		FoamTurbulentEnergyMultiplier)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		FoamWaveHatsMultiplier)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D, WindDirection)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,	WorldToClip)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D, ViewPortSize)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,		Time)
END_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineUniformParameters)
typedef TUniformBufferRef<FWaveWorksShorelineUniformParameters> FWaveWorksShorelineUniformBufferRef;

/**
* WaveWorks Vertex Structure
*/
struct FWaveWorksVertex
{
	FWaveWorksVertex()
		: Position(FVector2D::ZeroVector)
	{
	}

	FVector2D Position;
};

/**
* WaveWorks Vertex Buffer
*/
class FWaveWorksVertexBuffer : public FVertexBuffer
{
public:

	/** Vertices */
	TArray<FWaveWorksVertex> Vertices;

	/** Initialise vertex buffer */
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FWaveWorksVertex), BUF_Static, CreateInfo);

		void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FWaveWorksVertex), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof(FWaveWorksVertex));
		RHIUnlockVertexBuffer(VertexBufferRHI);

		Vertices.Empty();
	}
};

/**
* WaveWorks QuadTree Vertex Factory, inherit from LocalVertexFactory
*/
class FWaveWorksQuadTreeVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FWaveWorksQuadTreeVertexFactory)

public:
	/** Initialisation */
	void Init(const FWaveWorksVertexBuffer* VertexBuffer);

	/** Should cache */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/** Adds custom defines */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
};

/**
* WaveWorks Scene Proxy
*/
class ENGINE_API FWaveWorksSceneProxy : public FPrimitiveSceneProxy
{
public:
	
	/* Default constructor */
	FWaveWorksSceneProxy(class UWaveWorksComponent* InComponent, class UWaveWorks* InWaveWorks);
	~FWaveWorksSceneProxy();

	/* Begin FPrimitiveSceneProxy Interface */
	uint32 GetMemoryFootprint(void) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
	/* End FPrimitiveSceneProxy Interface */

	/** Sample Displacement with XY Plane's Sample Position */
	void SampleDisplacements_GameThread(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate VectorArrayDelegate);

	/** Get intersect point with ray */
	void GetIntersectPointWithRay_GameThread(FVector InOriginPoint, FVector InDirection, float SeaLevel, FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate);

	/** Attemp to create quad tree, when waveworks resource is ok */
	bool AttemptCreateQuadTree();

	/** Get WaveWorks QuadTree Handle */
	FORCEINLINE struct GFSDK_WaveWorks_Quadtree* GetQuadTreeHandle() { return QuadTreeHandle; }
	FORCEINLINE struct GFSDK_WaveWorks_Quadtree* GetQuadTreeHandle() const { return QuadTreeHandle; }

	/** Get WaveWorks Resource */
	FORCEINLINE class FWaveWorksResource* GetWaveWorksResource() const { return WaveWorksResource; }

	/** Get WaveWorks Asset */
	FORCEINLINE class UWaveWorks* GetWaveWorks() const { return WaveWorks; }

protected:
	FWaveWorksShorelineUniformBufferRef CreateShorelineUniformBuffer() const;

private:
	/** Quad Tree Handle*/
	struct GFSDK_WaveWorks_Quadtree* QuadTreeHandle;

	/** The WaveWorks object */
	class FWaveWorksResource* WaveWorksResource;

	/** The WaveWorks rendering material */
	class UMaterialInterface* WaveWorksMaterial;

	/** The WaveWorks Component */
	class UWaveWorksComponent* WaveWorksComponent;

	/** The WaveWorks Asset*/
	class UWaveWorks* WaveWorks;

	/** The WaveWorks QuadTree Vertex Factory */
	FWaveWorksQuadTreeVertexFactory VertexFactory;

	/** The WaveWorks Vertex Buffer */
	FWaveWorksVertexBuffer VertexBuffer;
};
