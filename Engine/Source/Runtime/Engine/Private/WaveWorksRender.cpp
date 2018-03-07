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

#include "WaveWorksRender.h"
#include "WaveWorksResource.h"
#include "Engine/WaveWorks.h"
#include "Engine/WaveWorksShorelineCapture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/WaveWorksComponent.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineUniformParameters, TEXT("WaveWorksShorelineParameters"));

/*=============================================================================
WaveWorksRender.cpp: FWaveWorksQuadTreeVertexFactory implementation.
=============================================================================*/
void FWaveWorksQuadTreeVertexFactory::Init(const FWaveWorksVertexBuffer* VertexBuffer)
{
	if (IsInRenderingThread())
	{
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FWaveWorksVertex, Position, VET_Float2);
		SetData(NewData);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitVertexFactory,
			FWaveWorksQuadTreeVertexFactory*, VertexFactory, this,
			const FWaveWorksVertexBuffer*, VertexBuffer, VertexBuffer,
			{
				FDataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FWaveWorksVertex, Position, VET_Float2);
				VertexFactory->SetData(NewData);
			}
		);
	}
}

bool FWaveWorksQuadTreeVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return true;
}

void FWaveWorksQuadTreeVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("WITH_GFSDK_WAVEWORKS"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT("WITH_GFSDK_QUAD_TREE_WAVEWORKS"), TEXT("1"));
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FWaveWorksQuadTreeVertexFactory, "/Engine/Private/LocalVertexFactory.ush", true, false, true, true, true);

/*=============================================================================
WaveWorksRender.cpp: FWaveWorksSceneProxy implementation.
=============================================================================*/

FWaveWorksSceneProxy::FWaveWorksSceneProxy(UWaveWorksComponent* InComponent, UWaveWorks* InWaveWorks)
	: FPrimitiveSceneProxy(InComponent)
	, WaveWorks(InWaveWorks)
	, WaveWorksComponent(InComponent)
	, QuadTreeHandle(nullptr)
{
	bVerifyUsedMaterials = false;

	bQuadTreeWaveWorks = true;
	if (nullptr != WaveWorksComponent)
		WaveWorksMaterial = WaveWorksComponent->WaveWorksMaterial;

	if (nullptr != WaveWorks)
	{
		WaveWorksResource = WaveWorks->GetWaveWorksResource();

		if (WaveWorksResource != nullptr)
			WaveWorksResource->CustomAddToDeferredUpdateList();
	}		

	/* Add temp vert */
	VertexBuffer.Vertices.Add(FWaveWorksVertex());
	VertexBuffer.Vertices.Add(FWaveWorksVertex());
	VertexBuffer.Vertices.Add(FWaveWorksVertex());

	VertexFactory.Init(&VertexBuffer);
	BeginInitResource(&VertexFactory);
	BeginInitResource(&VertexBuffer);	
}

FWaveWorksSceneProxy::~FWaveWorksSceneProxy()
{
	if (nullptr != QuadTreeHandle)
	{
		GFSDK_WaveWorks_Quadtree_Destroy(QuadTreeHandle);
		QuadTreeHandle = nullptr;
	}

	VertexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();

	if (WaveWorksResource != nullptr)
	{
		WaveWorksResource->CustomRemoveFromDeferredUpdateList();
	}
}

uint32 FWaveWorksSceneProxy::GetMemoryFootprint(void) const
{
	return 0;
}

FPrimitiveViewRelevance FWaveWorksSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	const bool bIsTranslucent = true;

	FPrimitiveViewRelevance ViewRelevance;
	ViewRelevance.bDrawRelevance = true;
	ViewRelevance.bDynamicRelevance = true;
	ViewRelevance.bRenderInMainPass = ShouldRenderInMainPass();
	ViewRelevance.bOpaqueRelevance = !bIsTranslucent;
	ViewRelevance.bNormalTranslucencyRelevance = bIsTranslucent;
	ViewRelevance.bSeparateTranslucencyRelevance = bIsTranslucent;
	ViewRelevance.bDistortionRelevance = bIsTranslucent;
	ViewRelevance.bRenderCustomDepth = ShouldRenderCustomDepth();
	ViewRelevance.bUsesGlobalDistanceField = WaveWorksComponent->bUsesGlobalDistanceField;
	ViewRelevance.bUsesSceneColorCopy = true;
	return ViewRelevance;
}

FWaveWorksShorelineUniformBufferRef FWaveWorksSceneProxy::CreateShorelineUniformBuffer() const
{	
	FWaveWorksShorelineUniformParameters WaveWorksShorelineUniformParameters;
	WaveWorksShorelineUniformParameters.bUseShoreline = WaveWorks->bUseShoreline ? 1 : 0;
	WaveWorksShorelineUniformParameters.GerstnerParallelity = WaveWorks->GerstnerParallelity;
	WaveWorksShorelineUniformParameters.MaxPixelsToShoreline = WaveWorks->MaxPixelsToShoreline;
	WaveWorksShorelineUniformParameters.FoamTurbulentEnergyMultiplier = WaveWorks->FoamTurbulentEnergyMultiplier;
	WaveWorksShorelineUniformParameters.FoamWaveHatsMultiplier = WaveWorks->FoamWaveHatsMultiplier;
	WaveWorksShorelineUniformParameters.WindDirection = -WaveWorks->WindDirection.GetSafeNormal();
	WaveWorksShorelineUniformParameters.Time = WaveWorks->GetShorelineTime();
	WaveWorksShorelineUniformParameters.GerstnerWaves = WaveWorks->GerstnerWaves;
	WaveWorksShorelineUniformParameters.GerstnerAmplitude = (WaveWorksResource != nullptr) ? WaveWorksResource->GetGerstnerAmplitude() * WaveWorks->GerstnerAmplitudeMultiplier : 0;
	WaveWorksShorelineUniformParameters.GerstnerSteepness = WaveWorks->GerstnerSteepness;
	// 7.0 is the min possible, according to Bascom reports: http://hyperphysics.phy-astr.gsu.edu/hbase/waves/watwav2.html                    
	WaveWorksShorelineUniformParameters.GerstnerWavelength = WaveWorksShorelineUniformParameters.GerstnerAmplitude * 14.0f * WaveWorks->GerstnerWaveLengthMultiplier;
	// m/sec, let's use equation for deep water waves for simplicity, and slow it down a bit as we're working with shallow water                        
	WaveWorksShorelineUniformParameters.GerstnerSpeed = FMath::Sqrt(9.81f * WaveWorksShorelineUniformParameters.GerstnerWavelength / 6.28f) * WaveWorks->GerstnerWaveSpeedMultiplier;
	
	FVector ViewLocation = WaveWorks->ShorelineCapturePosition;

	FRotator rotator(90, 0, 0);
	FMatrix ViewRotationMatrix = FRotationMatrix::Make(rotator);
	// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	float YAxisMultiplier;

	FIntPoint RenderTargetSize(WaveWorks->ShorelineDistanceFieldTexture->GetSurfaceWidth(), WaveWorks->ShorelineDistanceFieldTexture->GetSurfaceHeight());
	if (RenderTargetSize.X > RenderTargetSize.Y)
	{
		// if the viewport is wider than it is tall
		YAxisMultiplier = RenderTargetSize.X / (float)RenderTargetSize.Y;
	}
	else
	{
		// if the viewport is taller than it is wide
		YAxisMultiplier = 1.0f;
	}

	check((int32)ERHIZBuffer::IsInverted);
	const float OrthoWidth = WaveWorks->ShorelineCaptureOrthoSize / 2.0f;
	const float OrthoHeight = WaveWorks->ShorelineCaptureOrthoSize / 2.0f * YAxisMultiplier;

	const float NearPlane = 0;
	const float FarPlane = WORLD_MAX / 8.0f;

	const float ZScale = 1.0f / (FarPlane - NearPlane);
	const float ZOffset = -NearPlane;

	FMatrix ProjectionMatrix;
	ProjectionMatrix = FReversedZOrthoMatrix(
		OrthoWidth,
		OrthoHeight,
		ZScale,
		ZOffset
	);

	WaveWorksShorelineUniformParameters.WorldToClip = FTranslationMatrix(-ViewLocation) * ViewRotationMatrix * (ProjectionMatrix);
	WaveWorksShorelineUniformParameters.ViewPortSize = FVector2D(OrthoWidth * 2.0f / 100.0f, OrthoHeight * 2.0f / 100.0f);
	
	FWaveWorksShorelineUniformBufferRef ShorelineUniformBuffer = FWaveWorksShorelineUniformBufferRef::CreateUniformBufferImmediate(WaveWorksShorelineUniformParameters, UniformBuffer_SingleFrame);
	return ShorelineUniformBuffer;
}

void FWaveWorksSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			UMaterialInterface* WaveWorksMatInterface = WaveWorksComponent->GetMaterial(0);
			if (nullptr == WaveWorksMatInterface)
				WaveWorksMatInterface = WaveWorksMaterial;

			/* Create a dummy mesh (This will not actually be rendered) */
			FMeshBatch& Mesh = Collector.AllocateMesh();

			/* Create batch */
			Mesh.LODIndex = 0;
			Mesh.UseDynamicData = false;
			Mesh.DynamicVertexStride = 0;
			Mesh.DynamicVertexData = NULL;
			Mesh.VertexFactory = &VertexFactory;
			Mesh.ReverseCulling = true;
			Mesh.bDisableBackfaceCulling = false;
			Mesh.bWireframe = ViewFamily.EngineShowFlags.Wireframe;
			Mesh.Type = PT_3_ControlPointPatchList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.MaterialRenderProxy = WaveWorksMatInterface->GetRenderProxy(IsSelected(), IsHovered());

			/* Create element */
			FMeshBatchElement& BatchElement = Mesh.Elements[0];

			BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
			BatchElement.IndexBuffer = nullptr;
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = 1;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = 1;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}

	if (WaveWorksResource != nullptr)
	{
		WaveWorksResource->CustomAddToDeferredUpdateList();
		WaveWorksResource->SetShorelineUniformBuffer(CreateShorelineUniformBuffer());		
	}
}

void FWaveWorksSceneProxy::SampleDisplacements_GameThread(TArray<FVector> InSamplePoints, 
	FWaveWorksSampleDisplacementsDelegate VectorArrayDelegate)
{
	checkSlow(IsInGameThread());
	if (!WaveWorksResource)
		return;

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		SampleWaveWorksDisplacements,
		FWaveWorksRHIRef, WaveWorksRHI, WaveWorksResource->GetWaveWorksRHI(),
		TArray<FVector>, InSamplePoints, InSamplePoints,
		FWaveWorksSampleDisplacementsDelegate, OnRecieveDisplacementDelegate, VectorArrayDelegate,
		{
			if (NULL != WaveWorksRHI)
			{
				WaveWorksRHI->GetDisplacements(InSamplePoints, OnRecieveDisplacementDelegate);
			}
		});
}

void FWaveWorksSceneProxy::GetIntersectPointWithRay_GameThread(
	FVector InOriginPoint,
	FVector InDirection,
	float	SeaLevel,
	FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate)
{
	checkSlow(IsInGameThread());
	if (!WaveWorksResource)
		return;

	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER(
		GetIntersectPointWithRay,
		FWaveWorksRHIRef, WaveWorksRHI, WaveWorksResource->GetWaveWorksRHI(),
		FVector, InOriginPoint, InOriginPoint,
		FVector, InDirection, InDirection,
		float, SeaLevel, SeaLevel,
		FWaveWorksRaycastResultDelegate, OnRecieveIntersectPointDelegate, OnRecieveIntersectPointDelegate,
		{
			if (NULL != WaveWorksRHI)
			{
				WaveWorksRHI->GetIntersectPointWithRay(InOriginPoint, InDirection, SeaLevel, OnRecieveIntersectPointDelegate);
			}
		});
}

bool FWaveWorksSceneProxy::AttemptCreateQuadTree()
{
	bool bRet = false;

	WaveWorksResource = WaveWorks->GetWaveWorksResource();
	if (WaveWorksResource && !QuadTreeHandle)
	{
		FWaveWorksRHIRef WaveWorksRHI = WaveWorksResource->GetWaveWorksRHI();
		if (WaveWorksRHI)
		{
			WaveWorksRHI->CreateQuadTree(
				&QuadTreeHandle,
				WaveWorksComponent->MeshDim,
				WaveWorksComponent->MinPatchLength,
				(uint32)WaveWorksComponent->AutoRootLOD,
				WaveWorksComponent->UpperGridCoverage,
				WaveWorksComponent->SeaLevel,
				true,
				WaveWorksComponent->TessellationLOD,
				0);

			bRet = true;
		}
	}

	return bRet;
}