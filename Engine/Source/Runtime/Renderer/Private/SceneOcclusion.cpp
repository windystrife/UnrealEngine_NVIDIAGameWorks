// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.cpp: Scene rendering.
=============================================================================*/

#include "SceneOcclusion.h"
#include "EngineGlobals.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PlanarReflectionSceneProxy.h"
#include "PipelineStateCache.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

int32 GAllowPrecomputedVisibility = 1;
static FAutoConsoleVariableRef CVarAllowPrecomputedVisibility(
	TEXT("r.AllowPrecomputedVisibility"),
	GAllowPrecomputedVisibility,
	TEXT("If zero, precomputed visibility will not be used to cull primitives."),
	ECVF_RenderThreadSafe
	);

static int32 GShowPrecomputedVisibilityCells = 0;
static FAutoConsoleVariableRef CVarShowPrecomputedVisibilityCells(
	TEXT("r.ShowPrecomputedVisibilityCells"),
	GShowPrecomputedVisibilityCells,
	TEXT("If not zero, draw all precomputed visibility cells."),
	ECVF_RenderThreadSafe
	);

static int32 GShowRelevantPrecomputedVisibilityCells = 0;
static FAutoConsoleVariableRef CVarShowRelevantPrecomputedVisibilityCells(
	TEXT("r.ShowRelevantPrecomputedVisibilityCells"),
	GShowRelevantPrecomputedVisibilityCells,
	TEXT("If not zero, draw relevant precomputed visibility cells only."),
	ECVF_RenderThreadSafe
	);

int32 GOcclusionCullCascadedShadowMaps = 0;
FAutoConsoleVariableRef CVarOcclusionCullCascadedShadowMaps(
	TEXT("r.Shadow.OcclusionCullCascadedShadowMaps"),
	GOcclusionCullCascadedShadowMaps,
	TEXT("Whether to use occlusion culling on cascaded shadow maps.  Disabled by default because rapid view changes reveal new regions too quickly for latent occlusion queries to work with."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

#define NUM_CUBE_VERTICES 36

/** Random table for occlusion **/
FOcclusionRandomStream GOcclusionRandomStream;

int32 FOcclusionQueryHelpers::GetNumBufferedFrames()
{
#if WITH_SLI
	// If we're running with SLI, assume throughput is more important than latency, and buffer an extra frame
	check(GNumActiveGPUsForRendering <= (int32)FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
	return FMath::Min<int32>(GNumActiveGPUsForRendering, (int32)FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
#else
	static const auto NumBufferedQueriesVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.NumBufferedOcclusionQueries"));
	return FMath::Clamp<int32>(NumBufferedQueriesVar->GetValueOnAnyThread(), 1, (int32)FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
#endif
}


// default, non-instanced shader implementation
IMPLEMENT_SHADER_TYPE(,FOcclusionQueryVS,TEXT("/Engine/Private/OcclusionQueryVertexShader.usf"),TEXT("Main"),SF_Vertex);

static FGlobalBoundShaderState GOcclusionTestBoundShaderState;

/** 
 * Returns an array of visibility data for the given view position, or NULL if none exists. 
 * The data bits are indexed by VisibilityId of each primitive in the scene.
 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
 */
const uint8* FSceneViewState::GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene)
{
	const uint8* PrecomputedVisibilityData = NULL;
	if (Scene->PrecomputedVisibilityHandler && GAllowPrecomputedVisibility && View.Family->EngineShowFlags.PrecomputedVisibility)
	{
		const FPrecomputedVisibilityHandler& Handler = *Scene->PrecomputedVisibilityHandler;
		FViewElementPDI VisibilityCellsPDI(&View, NULL);

		// Draw visibility cell bounds for debugging if enabled
		if ((GShowPrecomputedVisibilityCells || View.Family->EngineShowFlags.PrecomputedVisibilityCells) && !GShowRelevantPrecomputedVisibilityCells)
		{
			for (int32 BucketIndex = 0; BucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
			{
				for (int32 CellIndex = 0; CellIndex < Handler.PrecomputedVisibilityCellBuckets[BucketIndex].Cells.Num(); CellIndex++)
				{
					const FPrecomputedVisibilityCell& CurrentCell = Handler.PrecomputedVisibilityCellBuckets[BucketIndex].Cells[CellIndex];
					// Construct the cell's bounds
					const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
					if (View.ViewFrustum.IntersectBox(CellBounds.GetCenter(), CellBounds.GetExtent()))
					{
						DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
					}
				}
			}
		}

		// Calculate the bucket that ViewOrigin falls into
		// Cells are hashed into buckets to reduce search time
		const float FloatOffsetX = (View.ViewMatrices.GetViewOrigin().X - Handler.PrecomputedVisibilityCellBucketOriginXY.X) / Handler.PrecomputedVisibilityCellSizeXY;
		// FMath::TruncToInt rounds toward 0, we want to always round down
		const int32 BucketIndexX = FMath::Abs((FMath::TruncToInt(FloatOffsetX) - (FloatOffsetX < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const float FloatOffsetY = (View.ViewMatrices.GetViewOrigin().Y -Handler.PrecomputedVisibilityCellBucketOriginXY.Y) / Handler.PrecomputedVisibilityCellSizeXY;
		const int32 BucketIndexY = FMath::Abs((FMath::TruncToInt(FloatOffsetY) - (FloatOffsetY < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const int32 PrecomputedVisibilityBucketIndex = BucketIndexY * Handler.PrecomputedVisibilityCellBucketSizeXY + BucketIndexX;

		check(PrecomputedVisibilityBucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num());
		const FPrecomputedVisibilityBucket& CurrentBucket = Handler.PrecomputedVisibilityCellBuckets[PrecomputedVisibilityBucketIndex];
		for (int32 CellIndex = 0; CellIndex < CurrentBucket.Cells.Num(); CellIndex++)
		{
			const FPrecomputedVisibilityCell& CurrentCell = CurrentBucket.Cells[CellIndex];
			// Construct the cell's bounds
			const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
			// Check if ViewOrigin is inside the current cell
			if (CellBounds.IsInside(View.ViewMatrices.GetViewOrigin()))
			{
				// Reuse a cached decompressed chunk if possible
				if (CachedVisibilityChunk
					&& CachedVisibilityHandlerId == Scene->PrecomputedVisibilityHandler->GetId()
					&& CachedVisibilityBucketIndex == PrecomputedVisibilityBucketIndex
					&& CachedVisibilityChunkIndex == CurrentCell.ChunkIndex)
				{
					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)[CurrentCell.DataOffset];
				}
				else
				{
					const FCompressedVisibilityChunk& CompressedChunk = Handler.PrecomputedVisibilityCellBuckets[PrecomputedVisibilityBucketIndex].CellDataChunks[CurrentCell.ChunkIndex];
					CachedVisibilityBucketIndex = PrecomputedVisibilityBucketIndex;
					CachedVisibilityChunkIndex = CurrentCell.ChunkIndex;
					CachedVisibilityHandlerId = Scene->PrecomputedVisibilityHandler->GetId();

					if (CompressedChunk.bCompressed)
					{
						// Decompress the needed visibility data chunk
						DecompressedVisibilityChunk.Reset();
						DecompressedVisibilityChunk.AddUninitialized(CompressedChunk.UncompressedSize);
						verify(FCompression::UncompressMemory(
							COMPRESS_ZLIB, 
							DecompressedVisibilityChunk.GetData(),
							CompressedChunk.UncompressedSize,
							CompressedChunk.Data.GetData(),
							CompressedChunk.Data.Num()));
						CachedVisibilityChunk = &DecompressedVisibilityChunk;
					}
					else
					{
						CachedVisibilityChunk = &CompressedChunk.Data;
					}

					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					// Return a pointer to the cell containing ViewOrigin's decompressed visibility data
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)[CurrentCell.DataOffset];
				}

				if (GShowRelevantPrecomputedVisibilityCells)
				{
					// Draw the currently used visibility cell with green wireframe for debugging
					DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 255, 50), SDPG_Foreground);
				}
				else
				{
					break;
				}
			}
			else if (GShowRelevantPrecomputedVisibilityCells)
			{
				// Draw all cells in the current visibility bucket as blue wireframe
				DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
			}
		}
	}
	return PrecomputedVisibilityData;
}

void FSceneViewState::TrimOcclusionHistory(FRHICommandListImmediate& RHICmdList, float CurrentTime, float MinHistoryTime, float MinQueryTime, int32 FrameNumber)
{
	// Only trim every few frames, since stale entries won't cause problems
	if (FrameNumber % 6 == 0)
	{
		int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();

		for(TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs>::TIterator PrimitiveIt(PrimitiveOcclusionHistorySet);
			PrimitiveIt;
			++PrimitiveIt
			)
		{
			// If the primitive has an old pending occlusion query, release it.
			if(PrimitiveIt->LastConsideredTime < MinQueryTime)
			{
				PrimitiveIt->ReleaseQueries(RHICmdList, OcclusionQueryPool, NumBufferedFrames);
			}

			// If the primitive hasn't been considered for visibility recently, remove its history from the set.
			if (PrimitiveIt->LastConsideredTime < MinHistoryTime || PrimitiveIt->LastConsideredTime > CurrentTime)
			{
				PrimitiveIt.RemoveCurrent();
			}
		}
	}
}

bool FSceneViewState::IsShadowOccluded(FRHICommandListImmediate& RHICmdList, FSceneViewState::FProjectedShadowKey ShadowKey, int32 NumBufferedFrames) const
{
	// Find the shadow's occlusion query from the previous frame.
	// Get the oldest occlusion query	
	const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(PendingPrevFrameNumber, NumBufferedFrames);
	const FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ShadowOcclusionQueryMaps[QueryIndex];	
	const FRenderQueryRHIRef* Query = ShadowOcclusionQueryMap.Find(ShadowKey);

	// Read the occlusion query results.
	uint64 NumSamples = 0;
	// Only block on the query if not running SLI
	const bool bWaitOnQuery = GNumActiveGPUsForRendering == 1;

	if (Query && RHICmdList.GetRenderQueryResult(*Query, NumSamples, bWaitOnQuery))
	{
		// If the shadow's occlusion query didn't have any pixels visible the previous frame, it's occluded.
		return NumSamples == 0;
	}
	else
	{
		// If the shadow wasn't queried the previous frame, it isn't occluded.

		return false;
	}
}

void FSceneViewState::Destroy()
{
	if ( IsInGameThread() )
	{
		// Release the occlusion query data.
		BeginReleaseResource(this);

		// Defer deletion of the view state until the rendering thread is done with it.
		BeginCleanup(this);
	}
	else
	{
		ReleaseResource();
		FinishCleanup();
	}
}

SIZE_T FSceneViewState::GetSizeBytes() const
{
	uint32 ShadowOcclusionQuerySize = ShadowOcclusionQueryMaps.GetAllocatedSize();
	for (int32 i = 0; i < ShadowOcclusionQueryMaps.Num(); ++i)
	{
		ShadowOcclusionQuerySize += ShadowOcclusionQueryMaps[i].GetAllocatedSize();
	}

	return sizeof(*this) 
		+ ShadowOcclusionQuerySize
		+ ParentPrimitives.GetAllocatedSize() 
		+ PrimitiveFadingStates.GetAllocatedSize()
		+ PrimitiveOcclusionHistorySet.GetAllocatedSize();
}

class FOcclusionQueryIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 MaxBatchedPrimitives = FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize;
		const uint32 Stride = sizeof(uint16);
		const uint32 SizeInBytes = MaxBatchedPrimitives * NUM_CUBE_VERTICES * Stride;

		FRHIResourceCreateInfo CreateInfo;

		void* BufferData;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(Stride, SizeInBytes, BUF_Static, CreateInfo, BufferData);
		uint16* RESTRICT Indices = (uint16*)BufferData;

		for(uint32 PrimitiveIndex = 0;PrimitiveIndex < MaxBatchedPrimitives;PrimitiveIndex++)
		{
			for(int32 Index = 0;Index < NUM_CUBE_VERTICES;Index++)
			{
				Indices[PrimitiveIndex * NUM_CUBE_VERTICES + Index] = PrimitiveIndex * 8 + GCubeIndices[Index];
			}
		}
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};
TGlobalResource<FOcclusionQueryIndexBuffer> GOcclusionQueryIndexBuffer;

FOcclusionQueryBatcher::FOcclusionQueryBatcher(class FSceneViewState* ViewState,uint32 InMaxBatchedPrimitives)
:	CurrentBatchOcclusionQuery(NULL)
,	MaxBatchedPrimitives(InMaxBatchedPrimitives)
,	NumBatchedPrimitives(0)
,	OcclusionQueryPool(ViewState ? &ViewState->OcclusionQueryPool : NULL)
{}

FOcclusionQueryBatcher::~FOcclusionQueryBatcher()
{
	check(!BatchOcclusionQueries.Num());
}

void FOcclusionQueryBatcher::Flush(FRHICommandListImmediate& RHICmdList)
{
	if(BatchOcclusionQueries.Num())
	{
		FMemMark MemStackMark(FMemStack::Get());

		// Create the indices for MaxBatchedPrimitives boxes.
		FIndexBufferRHIParamRef IndexBufferRHI = GOcclusionQueryIndexBuffer.IndexBufferRHI;

		// Draw the batches.
		for(int32 BatchIndex = 0, NumBatches = BatchOcclusionQueries.Num();BatchIndex < NumBatches;BatchIndex++)
		{
			FOcclusionBatch& Batch = BatchOcclusionQueries[BatchIndex];
			FRenderQueryRHIParamRef BatchOcclusionQuery = Batch.Query;
			FVertexBufferRHIParamRef VertexBufferRHI = Batch.VertexAllocation.VertexBuffer->VertexBufferRHI;
			uint32 VertexBufferOffset = Batch.VertexAllocation.VertexOffset;
			const int32 NumPrimitivesThisBatch = (BatchIndex != (NumBatches-1)) ? MaxBatchedPrimitives : NumBatchedPrimitives;
				
			RHICmdList.BeginRenderQuery(BatchOcclusionQuery);
			RHICmdList.SetStreamSource(0, VertexBufferRHI, VertexBufferOffset);
			RHICmdList.DrawIndexedPrimitive(
				IndexBufferRHI,
				PT_TriangleList,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 8 * NumPrimitivesThisBatch,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 12 * NumPrimitivesThisBatch,
				/*NumInstances=*/ 1
				);
			RHICmdList.EndRenderQuery(BatchOcclusionQuery);
		}
		INC_DWORD_STAT_BY(STAT_OcclusionQueries,BatchOcclusionQueries.Num());

		// Reset the batch state.
		BatchOcclusionQueries.Empty(BatchOcclusionQueries.Num());
		CurrentBatchOcclusionQuery = NULL;
	}
}

FRenderQueryRHIParamRef FOcclusionQueryBatcher::BatchPrimitive(const FVector& BoundsOrigin,const FVector& BoundsBoxExtent)
{
	// Check if the current batch is full.
	if(CurrentBatchOcclusionQuery == NULL || NumBatchedPrimitives >= MaxBatchedPrimitives)
	{
		check(OcclusionQueryPool);
		CurrentBatchOcclusionQuery = new(BatchOcclusionQueries) FOcclusionBatch;
		CurrentBatchOcclusionQuery->Query = OcclusionQueryPool->AllocateQuery();
		CurrentBatchOcclusionQuery->VertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(MaxBatchedPrimitives * 8 * sizeof(FVector));
		check(CurrentBatchOcclusionQuery->VertexAllocation.IsValid());
		NumBatchedPrimitives = 0;
	}

	// Add the primitive's bounding box to the current batch's vertex buffer.
	const FVector PrimitiveBoxMin = BoundsOrigin - BoundsBoxExtent;
	const FVector PrimitiveBoxMax = BoundsOrigin + BoundsBoxExtent;
	float* RESTRICT Vertices = (float*)CurrentBatchOcclusionQuery->VertexAllocation.Buffer;
	Vertices[ 0] = PrimitiveBoxMin.X; Vertices[ 1] = PrimitiveBoxMin.Y; Vertices[ 2] = PrimitiveBoxMin.Z;
	Vertices[ 3] = PrimitiveBoxMin.X; Vertices[ 4] = PrimitiveBoxMin.Y; Vertices[ 5] = PrimitiveBoxMax.Z;
	Vertices[ 6] = PrimitiveBoxMin.X; Vertices[ 7] = PrimitiveBoxMax.Y; Vertices[ 8] = PrimitiveBoxMin.Z;
	Vertices[ 9] = PrimitiveBoxMin.X; Vertices[10] = PrimitiveBoxMax.Y; Vertices[11] = PrimitiveBoxMax.Z;
	Vertices[12] = PrimitiveBoxMax.X; Vertices[13] = PrimitiveBoxMin.Y; Vertices[14] = PrimitiveBoxMin.Z;
	Vertices[15] = PrimitiveBoxMax.X; Vertices[16] = PrimitiveBoxMin.Y; Vertices[17] = PrimitiveBoxMax.Z;
	Vertices[18] = PrimitiveBoxMax.X; Vertices[19] = PrimitiveBoxMax.Y; Vertices[20] = PrimitiveBoxMin.Z;
	Vertices[21] = PrimitiveBoxMax.X; Vertices[22] = PrimitiveBoxMax.Y; Vertices[23] = PrimitiveBoxMax.Z;

	// Bump the batches buffer pointer.
	Vertices += 24;
	CurrentBatchOcclusionQuery->VertexAllocation.Buffer = (uint8*)Vertices;
	NumBatchedPrimitives++;

	return CurrentBatchOcclusionQuery->Query;
}

enum EShadowOcclusionQueryIntersectionMode
{
	SOQ_None,
	SOQ_LightInfluenceSphere,
	SOQ_NearPlaneVsShadowFrustum
};

static bool AllocateProjectedShadowOcclusionQuery(
	FViewInfo& View, 
	const FProjectedShadowInfo& ProjectedShadowInfo, 
	int32 NumBufferedFrames, 
	EShadowOcclusionQueryIntersectionMode IntersectionMode,
	FRenderQueryRHIRef& ShadowOcclusionQuery)
{
	bool bIssueQuery = true;

	if (IntersectionMode == SOQ_LightInfluenceSphere)
	{
	FLightSceneProxy& LightProxy = *(ProjectedShadowInfo.GetLightSceneInfo().Proxy);
	
	// Query one pass point light shadows separately because they don't have a shadow frustum, they have a bounding sphere instead.
	FSphere LightBounds = LightProxy.GetBoundingSphere();
	
	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);
		bIssueQuery = !bCameraInsideLightGeometry;
	}
	else if (IntersectionMode == SOQ_NearPlaneVsShadowFrustum)
	{
		// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to
		// be translated.
		const FVector4 PreShadowToPreViewTranslation(View.ViewMatrices.GetPreViewTranslation() - ProjectedShadowInfo.PreShadowTranslation,0);
	
		// If the shadow frustum is farther from the view origin than the near clipping plane,
		// it can't intersect the near clipping plane.
		const bool bIntersectsNearClippingPlane = ProjectedShadowInfo.ReceiverFrustum.IntersectSphere(
			View.ViewMatrices.GetViewOrigin() + ProjectedShadowInfo.PreShadowTranslation,
			View.NearClippingDistance * FMath::Sqrt(3.0f)
			);

		bIssueQuery = !bIntersectsNearClippingPlane;
	}

	if (bIssueQuery)
	{
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		// Allocate an occlusion query for the primitive from the occlusion query pool.
		ShadowOcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();

		FSceneViewState::FProjectedShadowKey Key(ProjectedShadowInfo);
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(ViewState->PendingPrevFrameNumber, NumBufferedFrames);
		FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMaps[QueryIndex];

		checkSlow(ShadowOcclusionQueryMap.Find(Key) == NULL);
		ShadowOcclusionQueryMap.Add(Key, ShadowOcclusionQuery);
	}
	
	return bIssueQuery;
}


static void ExecutePointLightShadowOcclusionQuery(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FProjectedShadowInfo& ProjectedShadowInfo, FOcclusionQueryVS* VertexShader, FRenderQueryRHIRef ShadowOcclusionQuery)
{
	FLightSceneProxy& LightProxy = *(ProjectedShadowInfo.GetLightSceneInfo().Proxy);
	
	// Query one pass point light shadows separately because they don't have a shadow frustum, they have a bounding sphere instead.
	FSphere LightBounds = LightProxy.GetBoundingSphere();

	RHICmdList.BeginRenderQuery(ShadowOcclusionQuery);
	
	// Draw bounding sphere
	VertexShader->SetParametersWithBoundingSphere(RHICmdList, View, LightBounds);
	StencilingGeometry::DrawVectorSphere(RHICmdList);
		
	RHICmdList.EndRenderQuery(ShadowOcclusionQuery);
}

static void ExecuteDirectionalLightShadowOcclusionQuery(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FProjectedShadowInfo& ProjectedShadowInfo, FOcclusionQueryVS* VertexShader, FRenderQueryRHIRef ShadowOcclusionQuery)
{
	RHICmdList.BeginRenderQuery(ShadowOcclusionQuery);
	
	// Draw bounding sphere
	VertexShader->SetParameters(RHICmdList, View);
	
	const FMatrix& ViewMatrix = View.ShadowViewMatrices.GetViewMatrix();
	const FMatrix& ProjectionMatrix = View.ShadowViewMatrices.GetProjectionMatrix();
	const FVector CameraDirection = ViewMatrix.GetColumn(2);
	const float SplitNear = ProjectedShadowInfo.CascadeSettings.SplitNear;

	float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
	float HalfFOV = View.ShadowViewMatrices.IsPerspectiveProjection() ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI / 4.0f;

	// Build the camera frustum for this cascade
	const float StartHorizontalLength = SplitNear * FMath::Tan(HalfFOV);
	const FVector StartCameraRightOffset = ViewMatrix.GetColumn(0) * StartHorizontalLength;
	const float StartVerticalLength = StartHorizontalLength / AspectRatio;
	const FVector StartCameraUpOffset = ViewMatrix.GetColumn(1) * StartVerticalLength;

	FVector Verts[4] =
	{
		CameraDirection * SplitNear + StartCameraRightOffset + StartCameraUpOffset,
		CameraDirection * SplitNear + StartCameraRightOffset - StartCameraUpOffset,
		CameraDirection * SplitNear - StartCameraRightOffset - StartCameraUpOffset,
		CameraDirection * SplitNear - StartCameraRightOffset + StartCameraUpOffset
	};

	FVector TriangleVerts[6] = 
	{
		Verts[0],
		Verts[3],
		Verts[2],
		Verts[0],
		Verts[2],
		Verts[1],
	};

	DrawPrimitiveUP(RHICmdList, PT_TriangleList, 2, TriangleVerts, sizeof(FVector));

	RHICmdList.EndRenderQuery(ShadowOcclusionQuery);
}

static void ExecuteProjectedShadowOcclusionQuery(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FProjectedShadowInfo& ProjectedShadowInfo, FOcclusionQueryVS* VertexShader, FRenderQueryRHIRef ShadowOcclusionQuery)
{	
	// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to
	// be translated.
	const FVector4 PreShadowToPreViewTranslation(View.ViewMatrices.GetPreViewTranslation() - ProjectedShadowInfo.PreShadowTranslation,0);

	VertexShader->SetParameters(RHICmdList, View);

	// Draw the primitive's bounding box, using the occlusion query.
	RHICmdList.BeginRenderQuery(ShadowOcclusionQuery);

	void* VerticesPtr;
	void* IndicesPtr;
	// preallocate memory to fill out with vertices and indices
	RHICmdList.BeginDrawIndexedPrimitiveUP(PT_TriangleList, 12, 8, sizeof(FVector), VerticesPtr, 0, NUM_CUBE_VERTICES, sizeof(uint16), IndicesPtr);
	FVector* Vertices = (FVector*)VerticesPtr;
	uint16* Indices = (uint16*)IndicesPtr;

	// Generate vertices for the shadow's frustum.
	for(uint32 Z = 0;Z < 2;Z++)
	{
		for(uint32 Y = 0;Y < 2;Y++)
		{
			for(uint32 X = 0;X < 2;X++)
			{
				const FVector4 UnprojectedVertex = ProjectedShadowInfo.InvReceiverMatrix.TransformFVector4(
					FVector4(
					(X ? -1.0f : 1.0f),
					(Y ? -1.0f : 1.0f),
					(Z ?  1.0f : 0.0f),
					1.0f
					)
					);
				const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
				Vertices[GetCubeVertexIndex(X,Y,Z)] = ProjectedVertex;
			}
		}
	}

	// we just copy the indices right in
	FMemory::Memcpy(Indices, GCubeIndices, sizeof(GCubeIndices));

	RHICmdList.EndDrawIndexedPrimitiveUP();
	RHICmdList.EndRenderQuery(ShadowOcclusionQuery);
}

static bool AllocatePlanarReflectionOcclusionQuery(FViewInfo& View, const FPlanarReflectionSceneProxy* SceneProxy, int32 NumBufferedFrames, FRenderQueryRHIRef& OcclusionQuery)
{
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	bool bAllowBoundsTest = false;
	
	if (View.ViewFrustum.IntersectBox(SceneProxy->WorldBounds.GetCenter(), SceneProxy->WorldBounds.GetExtent()))
	{
		const FBoxSphereBounds OcclusionBounds(SceneProxy->WorldBounds);
		
		if (View.bHasNearClippingPlane)
		{
			bAllowBoundsTest = View.NearClippingPlane.PlaneDot(OcclusionBounds.Origin) <
			-(FVector::BoxPushOut(View.NearClippingPlane, OcclusionBounds.BoxExtent));
			
		}
		else if (!View.IsPerspectiveProjection())
		{
			// Transform parallel near plane
			static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check equation for culling!");
			bAllowBoundsTest = View.WorldToScreen(OcclusionBounds.Origin).Z - View.ViewMatrices.GetProjectionMatrix().M[2][2] * OcclusionBounds.SphereRadius < 1;
		}
		else
		{
			bAllowBoundsTest = OcclusionBounds.SphereRadius < HALF_WORLD_MAX;
		}
	}
	
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	FIndividualOcclusionHistory& OcclusionHistory = ViewState->PlanarReflectionOcclusionHistories.FindOrAdd(SceneProxy->PlanarReflectionId);
	ViewState->OcclusionQueryPool.ReleaseQuery(OcclusionHistory.GetPastQuery(OcclusionFrameCounter, NumBufferedFrames));
	
	if (bAllowBoundsTest)
	{
		// Allocate an occlusion query for the primitive from the occlusion query pool.
		OcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();
		
		OcclusionHistory.SetCurrentQuery(OcclusionFrameCounter, OcclusionQuery, NumBufferedFrames);
	}
	else
	{
		OcclusionHistory.SetCurrentQuery(OcclusionFrameCounter, NULL, NumBufferedFrames);
	}
	
	return bAllowBoundsTest;
}

static void ExecutePlanarReflectionOcclusionQuery(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FPlanarReflectionSceneProxy* SceneProxy, FOcclusionQueryVS* VertexShader, FRenderQueryRHIRef OcclusionQuery)
{
	VertexShader->SetParameters(RHICmdList, View);

	// Draw the primitive's bounding box, using the occlusion query.
	RHICmdList.BeginRenderQuery(OcclusionQuery);

	void* VerticesPtr;
	void* IndicesPtr;
	// preallocate memory to fill out with vertices and indices
	RHICmdList.BeginDrawIndexedPrimitiveUP(PT_TriangleList, 12, 8, sizeof(FVector), VerticesPtr, 0, NUM_CUBE_VERTICES, sizeof(uint16), IndicesPtr);
	float* Vertices = (float*)VerticesPtr;
	uint16* Indices = (uint16*)IndicesPtr;

	const FVector PrimitiveBoxMin = SceneProxy->WorldBounds.Min + View.ViewMatrices.GetPreViewTranslation();
	const FVector PrimitiveBoxMax = SceneProxy->WorldBounds.Max + View.ViewMatrices.GetPreViewTranslation();
	Vertices[0] = PrimitiveBoxMin.X; Vertices[1] = PrimitiveBoxMin.Y; Vertices[2] = PrimitiveBoxMin.Z;
	Vertices[3] = PrimitiveBoxMin.X; Vertices[4] = PrimitiveBoxMin.Y; Vertices[5] = PrimitiveBoxMax.Z;
	Vertices[6] = PrimitiveBoxMin.X; Vertices[7] = PrimitiveBoxMax.Y; Vertices[8] = PrimitiveBoxMin.Z;
	Vertices[9] = PrimitiveBoxMin.X; Vertices[10] = PrimitiveBoxMax.Y; Vertices[11] = PrimitiveBoxMax.Z;
	Vertices[12] = PrimitiveBoxMax.X; Vertices[13] = PrimitiveBoxMin.Y; Vertices[14] = PrimitiveBoxMin.Z;
	Vertices[15] = PrimitiveBoxMax.X; Vertices[16] = PrimitiveBoxMin.Y; Vertices[17] = PrimitiveBoxMax.Z;
	Vertices[18] = PrimitiveBoxMax.X; Vertices[19] = PrimitiveBoxMax.Y; Vertices[20] = PrimitiveBoxMin.Z;
	Vertices[21] = PrimitiveBoxMax.X; Vertices[22] = PrimitiveBoxMax.Y; Vertices[23] = PrimitiveBoxMax.Z;

	FMemory::Memcpy(Indices, GCubeIndices, sizeof(GCubeIndices));

	RHICmdList.EndDrawIndexedPrimitiveUP();
	RHICmdList.EndRenderQuery(OcclusionQuery);
}

FHZBOcclusionTester::FHZBOcclusionTester()
	: ResultsBuffer( NULL )
{
	SetInvalidFrameNumber();
}

bool FHZBOcclusionTester::IsValidFrame(uint32 FrameNumber) const
{
	return (FrameNumber & FrameNumberMask) == ValidFrameNumber;
}

void FHZBOcclusionTester::SetValidFrameNumber(uint32 FrameNumber)
{
	ValidFrameNumber = FrameNumber & FrameNumberMask;

	checkSlow(!IsInvalidFrame());
}

bool FHZBOcclusionTester::IsInvalidFrame() const
{
	return ValidFrameNumber == InvalidFrameNumber;
}

void FHZBOcclusionTester::SetInvalidFrameNumber()
{
	// this number cannot be set by SetValidFrameNumber()
	ValidFrameNumber = InvalidFrameNumber;

	checkSlow(IsInvalidFrame());
}

void FHZBOcclusionTester::InitDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture, TexCreate_None, false ) );
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ResultsTextureCPU, TEXT("HZBResultsCPU"), true, ERenderTargetTransience::NonTransient );
	}
}

void FHZBOcclusionTester::ReleaseDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		GRenderTargetPool.FreeUnusedResource( ResultsTextureCPU );
	}
}

uint32 FHZBOcclusionTester::AddBounds( const FVector& BoundsCenter, const FVector& BoundsExtent )
{
	uint32 Index = Primitives.AddUninitialized();
	check( Index < SizeX * SizeY );
	Primitives[ Index ].Center = BoundsCenter;
	Primitives[ Index ].Extent = BoundsExtent;
	return Index;
}

void FHZBOcclusionTester::MapResults(FRHICommandListImmediate& RHICmdList)
{
	check( !ResultsBuffer );

	if( !IsInvalidFrame() )
	{
		uint32 IdleStart = FPlatformTime::Cycles();

		int32 Width = 0;
		int32 Height = 0;

		RHICmdList.MapStagingSurface(ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture, *(void**)&ResultsBuffer, Width, Height);

		// RHIMapStagingSurface will block until the results are ready (from the previous frame) so we need to consider this RT idle time
		GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
	}
	
	// Can happen because of device removed, we might crash later but this occlusion culling system can behave gracefully.
	if( ResultsBuffer == NULL )
	{
		// First frame
		static uint8 FirstFrameBuffer[] = { 255 };
		ResultsBuffer = FirstFrameBuffer;
		SetInvalidFrameNumber();
	}
}

void FHZBOcclusionTester::UnmapResults(FRHICommandListImmediate& RHICmdList)
{
	check( ResultsBuffer );
	if(!IsInvalidFrame())
	{
		RHICmdList.UnmapStagingSurface(ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture);
	}
	ResultsBuffer = NULL;
}

bool FHZBOcclusionTester::IsVisible( uint32 Index ) const
{
	checkSlow( ResultsBuffer );
	checkSlow( Index < SizeX * SizeY );
	
	// TODO shader compress to bits

#if 0
	return ResultsBuffer[ 4 * Index ] != 0;
#elif 0
	uint32 x = FMath::ReverseMortonCode2( Index >> 0 );
	uint32 y = FMath::ReverseMortonCode2( Index >> 1 );
	uint32 m = x + y * SizeX;
	return ResultsBuffer[ 4 * m ] != 0;
#else
	// TODO put block constants in class
	// TODO optimize
	const uint32 BlockSize = 8;
	const uint32 SizeInBlocksX = SizeX / BlockSize;
	const uint32 SizeInBlocksY = SizeY / BlockSize;

	const int32 BlockIndex = Index / (BlockSize * BlockSize);
	const int32 BlockX = BlockIndex % SizeInBlocksX;
	const int32 BlockY = BlockIndex / SizeInBlocksY;

	const int32 b = Index % (BlockSize * BlockSize);
	const int32 x = BlockX * BlockSize + b % BlockSize;
	const int32 y = BlockY * BlockSize + b / BlockSize;

	return ResultsBuffer[ 4 * (x + y * SizeY) ] != 0;
#endif
}

class FHZBTestPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHZBTestPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FHZBTestPS() {}

public:
	FShaderParameter				HZBUvFactor;
	FShaderParameter				HZBSize;
	FShaderResourceParameter		HZBTexture;
	FShaderResourceParameter		HZBSampler;
	FShaderResourceParameter		BoundsCenterTexture;
	FShaderResourceParameter		BoundsCenterSampler;
	FShaderResourceParameter		BoundsExtentTexture;
	FShaderResourceParameter		BoundsExtentSampler;

	FHZBTestPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HZBUvFactor.Bind( Initializer.ParameterMap, TEXT("HZBUvFactor") );
		HZBSize.Bind( Initializer.ParameterMap, TEXT("HZBSize") );
		HZBTexture.Bind( Initializer.ParameterMap, TEXT("HZBTexture") );
		HZBSampler.Bind( Initializer.ParameterMap, TEXT("HZBSampler") );
		BoundsCenterTexture.Bind( Initializer.ParameterMap, TEXT("BoundsCenterTexture") );
		BoundsCenterSampler.Bind( Initializer.ParameterMap, TEXT("BoundsCenterSampler") );
		BoundsExtentTexture.Bind( Initializer.ParameterMap, TEXT("BoundsExtentTexture") );
		BoundsExtentSampler.Bind( Initializer.ParameterMap, TEXT("BoundsExtentSampler") );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FTextureRHIParamRef BoundsCenter, FTextureRHIParamRef BoundsExtent )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		/*
		 * Defines the maximum number of mipmaps the HZB test is considering
		 * to avoid memory cache trashing when rendering on high resolution.
		 */
		const float kHZBTestMaxMipmap = 9.0f;

		const float HZBMipmapCounts = FMath::Log2(FMath::Max(View.HZBMipmap0Size.X, View.HZBMipmap0Size.Y));
		const FVector HZBUvFactorValue(
			float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
			float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y),
			FMath::Max(HZBMipmapCounts - kHZBTestMaxMipmap, 0.0f)
			);
		const FVector4 HZBSizeValue(
			View.HZBMipmap0Size.X,
			View.HZBMipmap0Size.Y,
			1.0f / float(View.HZBMipmap0Size.X),
			1.0f / float(View.HZBMipmap0Size.Y)
			);
		SetShaderValue(RHICmdList, ShaderRHI, HZBUvFactor, HZBUvFactorValue);
		SetShaderValue(RHICmdList, ShaderRHI, HZBSize, HZBSizeValue);

		SetTextureParameter(RHICmdList, ShaderRHI, HZBTexture, HZBSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), View.HZB->GetRenderTargetItem().ShaderResourceTexture );

		SetTextureParameter(RHICmdList, ShaderRHI, BoundsCenterTexture, BoundsCenterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), BoundsCenter );
		SetTextureParameter(RHICmdList, ShaderRHI, BoundsExtentTexture, BoundsExtentSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), BoundsExtent );
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HZBUvFactor;
		Ar << HZBSize;
		Ar << HZBTexture;
		Ar << HZBSampler;
		Ar << BoundsCenterTexture;
		Ar << BoundsCenterSampler;
		Ar << BoundsExtentTexture;
		Ar << BoundsExtentSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FHZBTestPS,TEXT("/Engine/Private/HZBOcclusion.usf"),TEXT("HZBTestPS"),SF_Pixel);

void FHZBOcclusionTester::Submit(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	SCOPED_DRAW_EVENT(RHICmdList, SubmitHZB);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	if( !ViewState )
	{
		return;
	}

	TRefCountPtr< IPooledRenderTarget >	BoundsCenterTexture;
	TRefCountPtr< IPooledRenderTarget >	BoundsExtentTexture;
	{
		uint32 Flags = TexCreate_ShaderResource | TexCreate_Dynamic;
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_A32B32G32R32F, FClearValueBinding::None, Flags, TexCreate_None, false ) );

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, BoundsCenterTexture, TEXT("HZBBoundsCenter") );
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, BoundsExtentTexture, TEXT("HZBBoundsExtent") );
	}

	TRefCountPtr< IPooledRenderTarget >	ResultsTextureGPU;
	{
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false ) );
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ResultsTextureGPU, TEXT("HZBResultsGPU") );
	}

	{
#if 0
		static float CenterBuffer[ SizeX * SizeY ][4];
		static float ExtentBuffer[ SizeX * SizeY ][4];

		FMemory::Memset( CenterBuffer, 0, sizeof( CenterBuffer ) );
		FMemory::Memset( ExtentBuffer, 0, sizeof( ExtentBuffer ) );

		const uint32 NumPrimitives = Primitives.Num();
		for( uint32 i = 0; i < NumPrimitives; i++ )
		{
			const FOcclusionPrimitive& Primitive = Primitives[i];

			CenterBuffer[i][0] = Primitive.Center.X;
			CenterBuffer[i][1] = Primitive.Center.Y;
			CenterBuffer[i][2] = Primitive.Center.Z;
			CenterBuffer[i][3] = 0.0f;

			ExtentBuffer[i][0] = Primitive.Extent.X;
			ExtentBuffer[i][1] = Primitive.Extent.Y;
			ExtentBuffer[i][2] = Primitive.Extent.Z;
			ExtentBuffer[i][3] = 1.0f;
		}

		FUpdateTextureRegion2D Region( 0, 0, 0, 0, SizeX, SizeY );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)CenterBuffer );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)ExtentBuffer );
#elif 0
		static float CenterBuffer[ SizeX * SizeY ][4];
		static float ExtentBuffer[ SizeX * SizeY ][4];

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBPackPrimitiveData);
			
			FMemory::Memset( CenterBuffer, 0, sizeof( CenterBuffer ) );
			FMemory::Memset( ExtentBuffer, 0, sizeof( ExtentBuffer ) );

			const uint32 NumPrimitives = Primitives.Num();
			for( uint32 i = 0; i < NumPrimitives; i++ )
			{
				const FOcclusionPrimitive& Primitive = Primitives[i];

				uint32 x = FMath::ReverseMortonCode2( i >> 0 );
				uint32 y = FMath::ReverseMortonCode2( i >> 1 );
				uint32 m = x + y * SizeX;

				CenterBuffer[m][0] = Primitive.Center.X;
				CenterBuffer[m][1] = Primitive.Center.Y;
				CenterBuffer[m][2] = Primitive.Center.Z;
				CenterBuffer[m][3] = 0.0f;

				ExtentBuffer[m][0] = Primitive.Extent.X;
				ExtentBuffer[m][1] = Primitive.Extent.Y;
				ExtentBuffer[m][2] = Primitive.Extent.Z;
				ExtentBuffer[m][3] = 1.0f;
			}
		}
		
		QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUpdateTextures);
		FUpdateTextureRegion2D Region( 0, 0, 0, 0, SizeX, SizeY );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)CenterBuffer );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)ExtentBuffer );
#else
		// Update in blocks to avoid large update
		const uint32 BlockSize = 8;
		const uint32 SizeInBlocksX = SizeX / BlockSize;
		const uint32 SizeInBlocksY = SizeY / BlockSize;
		const uint32 BlockStride = BlockSize * 4 * sizeof( float );

		float CenterBuffer[ BlockSize * BlockSize ][4];
		float ExtentBuffer[ BlockSize * BlockSize ][4];

		const uint32 NumPrimitives = Primitives.Num();
		for( uint32 i = 0; i < NumPrimitives; i += BlockSize * BlockSize )
		{
			const uint32 BlockEnd = FMath::Min( BlockSize * BlockSize, NumPrimitives - i );
			for( uint32 b = 0; b < BlockEnd; b++ )
			{
				const FOcclusionPrimitive& Primitive = Primitives[ i + b ];

				CenterBuffer[b][0] = Primitive.Center.X;
				CenterBuffer[b][1] = Primitive.Center.Y;
				CenterBuffer[b][2] = Primitive.Center.Z;
				CenterBuffer[b][3] = 0.0f;

				ExtentBuffer[b][0] = Primitive.Extent.X;
				ExtentBuffer[b][1] = Primitive.Extent.Y;
				ExtentBuffer[b][2] = Primitive.Extent.Z;
				ExtentBuffer[b][3] = 1.0f;
			}

			// Clear rest of block
			if( BlockEnd < BlockSize * BlockSize )
			{
				FMemory::Memset( (float*)CenterBuffer + BlockEnd * 4, 0, sizeof( CenterBuffer ) - BlockEnd * 4 * sizeof(float) );
				FMemory::Memset( (float*)ExtentBuffer + BlockEnd * 4, 0, sizeof( ExtentBuffer ) - BlockEnd * 4 * sizeof(float) );
			}

			const int32 BlockIndex = i / (BlockSize * BlockSize);
			const int32 BlockX = BlockIndex % SizeInBlocksX;
			const int32 BlockY = BlockIndex / SizeInBlocksY;

			FUpdateTextureRegion2D Region( BlockX * BlockSize, BlockY * BlockSize, 0, 0, BlockSize, BlockSize );
			RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, BlockStride, (uint8*)CenterBuffer );
			RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, BlockStride, (uint8*)ExtentBuffer );
		}
#endif
		Primitives.Empty();
	}

	// Draw test
	{
		SCOPED_DRAW_EVENT(RHICmdList, TestHZB);

		SetRenderTarget(RHICmdList, ResultsTextureGPU->GetRenderTargetItem().TargetableTexture, NULL);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef< FScreenVS >	VertexShader(View.ShaderMap);
		TShaderMapRef< FHZBTestPS >	PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, View, BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture );

		RHICmdList.SetViewport(0, 0, 0.0f, SizeX, SizeY, 1.0f);

		// TODO draw quads covering blocks added above
		DrawRectangle(
			RHICmdList,
			0, 0,
			SizeX, SizeY,
			0, 0,
			SizeX, SizeY,
			FIntPoint( SizeX, SizeY ),
			FIntPoint( SizeX, SizeY ),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ResultsTextureGPU);

	// Transfer memory GPU -> CPU
	RHICmdList.CopyToResolveTarget(ResultsTextureGPU->GetRenderTargetItem().TargetableTexture, ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
}

template< uint32 Stage >
class THZBBuildPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(THZBBuildPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("STAGE"), Stage );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R32_FLOAT);
	}

	THZBBuildPS() {}

public:
	FShaderParameter				InvSizeParameter;
	FShaderParameter				InputUvFactorAndOffsetParameter;
	FShaderParameter				InputViewportMaxBoundParameter;
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderResourceParameter		TextureParameter;
	FShaderResourceParameter		TextureParameterSampler;

	THZBBuildPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InvSizeParameter.Bind( Initializer.ParameterMap, TEXT("InvSize") );
		InputUvFactorAndOffsetParameter.Bind( Initializer.ParameterMap, TEXT("InputUvFactorAndOffset") );
		InputViewportMaxBoundParameter.Bind( Initializer.ParameterMap, TEXT("InputViewportMaxBound") );
		SceneTextureParameters.Bind( Initializer.ParameterMap );
		TextureParameter.Bind( Initializer.ParameterMap, TEXT("Texture") );
		TextureParameterSampler.Bind( Initializer.ParameterMap, TEXT("TextureSampler") );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		
		const FIntPoint GBufferSize = SceneContext.GetBufferSizeXY();
		const FVector2D InvSize( 1.0f / float(GBufferSize.X), 1.0f / float(GBufferSize.Y) );
		const FVector4 InputUvFactorAndOffset (
			float(2 * View.HZBMipmap0Size.X) / float(GBufferSize.X),
			float(2 * View.HZBMipmap0Size.Y) / float(GBufferSize.Y),
			float(View.ViewRect.Min.X) / float(GBufferSize.X),
			float(View.ViewRect.Min.Y) / float(GBufferSize.Y)
			);
		const FVector2D InputViewportMaxBound (
			float(View.ViewRect.Max.X) / float(GBufferSize.X) - 0.5f * InvSize.X,
			float(View.ViewRect.Max.Y) / float(GBufferSize.Y) - 0.5f * InvSize.Y
			);
		SetShaderValue(RHICmdList, ShaderRHI, InvSizeParameter, InvSize );
		SetShaderValue(RHICmdList, ShaderRHI, InputUvFactorAndOffsetParameter, InputUvFactorAndOffset );
		SetShaderValue(RHICmdList, ShaderRHI, InputViewportMaxBoundParameter, InputViewportMaxBound );
		
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FIntPoint& Size, FShaderResourceViewRHIParamRef ShaderResourceView )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		const FVector2D InvSize( 1.0f / Size.X, 1.0f / Size.Y );
		SetShaderValue(RHICmdList, ShaderRHI, InvSizeParameter, InvSize );

		//SetTextureParameter( ShaderRHI, TextureParameter, TextureParameterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Texture );

		SetSRVParameter(RHICmdList, ShaderRHI, TextureParameter, ShaderResourceView );
		SetSamplerParameter(RHICmdList, ShaderRHI, TextureParameterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI() );
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvSizeParameter;
		Ar << InputUvFactorAndOffsetParameter;
		Ar << InputViewportMaxBoundParameter;
		Ar << SceneTextureParameters;
		Ar << TextureParameter;
		Ar << TextureParameterSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(template<>,THZBBuildPS<0>,TEXT("/Engine/Private/HZBOcclusion.usf"),TEXT("HZBBuildPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,THZBBuildPS<1>,TEXT("/Engine/Private/HZBOcclusion.usf"),TEXT("HZBBuildPS"),SF_Pixel);

void BuildHZB( FRHICommandListImmediate& RHICmdList, FViewInfo& View )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BuildHZB);
	
	// View.ViewRect.{Width,Height}() are most likely to be < 2^24, so the float
	// conversion won't loss any precision (assuming float have 23bits for mantissa)
	const int32 NumMipsX = FMath::Max(FPlatformMath::CeilToInt(FMath::Log2(float(View.ViewRect.Width()))) - 1, 1);
	const int32 NumMipsY = FMath::Max(FPlatformMath::CeilToInt(FMath::Log2(float(View.ViewRect.Height()))) - 1, 1);
	const uint32 NumMips = FMath::Max(NumMipsX, NumMipsY);

	// Must be power of 2
	const FIntPoint HZBSize( 1 << NumMipsX, 1 << NumMipsY );
	View.HZBMipmap0Size = HZBSize;

	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(HZBSize, PF_R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_NoFastClear, false, NumMips));
	Desc.Flags |= GFastVRamConfig.HZB;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, View.HZB, TEXT("HZB") );
	
	FSceneRenderTargetItem& HZBRenderTarget = View.HZB->GetRenderTargetItem();
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FTextureRHIParamRef HZBRenderTargetRef = HZBRenderTarget.TargetableTexture.GetReference();
	// Mip 0
	{
		SCOPED_DRAW_EVENTF(RHICmdList, BuildHZB, TEXT("HZB SetupMip 0 %dx%d"), HZBSize.X, HZBSize.Y);

		SetRenderTarget(RHICmdList, HZBRenderTarget.TargetableTexture, 0, NULL);
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef< FPostProcessVS >	VertexShader(View.ShaderMap);
		TShaderMapRef< THZBBuildPS<0> >	PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		// Imperfect sampling, doesn't matter too much
		PixelShader->SetParameters( RHICmdList, View );

		RHICmdList.SetViewport(0, 0, 0.0f, HZBSize.X, HZBSize.Y, 1.0f);

		DrawRectangle(
			RHICmdList,
			0, 0,
			HZBSize.X, HZBSize.Y,
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			HZBSize,
			FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	FIntPoint SrcSize = HZBSize;
	FIntPoint DstSize = SrcSize / 2;

	SCOPED_DRAW_EVENTF(RHICmdList, BuildHZB, TEXT("HZB SetupMips Mips:1..%d %dx%d"), NumMips - 1, DstSize.X, DstSize.Y);

	//Use RWBarrier since we don't transition individual subresources.  Basically treat the whole texture as R/W as we walk down the mip chain.
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWSubResBarrier, &HZBRenderTargetRef, 1);

	TShaderMapRef< FPostProcessVS >	VertexShader(View.ShaderMap);
	TShaderMapRef< THZBBuildPS<1> >	PixelShader(View.ShaderMap);

	// Downsampling...
	for (uint8 MipIndex = 1; MipIndex < NumMips; MipIndex++)
	{
		DstSize.X = FMath::Max(DstSize.X, 1);
		DstSize.Y = FMath::Max(DstSize.Y, 1);

		SetRenderTarget(RHICmdList, HZBRenderTarget.TargetableTexture, MipIndex, NULL);
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, View, SrcSize, HZBRenderTarget.MipSRVs[MipIndex - 1]);

		RHICmdList.SetViewport(0, 0, 0.0f, DstSize.X, DstSize.Y, 1.0f);

		DrawRectangle(
			RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			0, 0,
			SrcSize.X, SrcSize.Y,
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);

		SrcSize /= 2;
		DstSize /= 2;

		//Use ERWSubResBarrier since we don't transition individual subresources.  Basically treat the whole texture as R/W as we walk down the mip chain.
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWSubResBarrier, &HZBRenderTargetRef, 1);
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint( RHICmdList, View.HZB );
}

void FDeferredShadingSceneRenderer::BeginOcclusionTests(FRHICommandListImmediate& RHICmdList, bool bRenderQueries)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_BeginOcclusionTests, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_BeginOcclusionTestsTime);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const bool bUseDownsampledDepth = SceneContext.UseDownsizedOcclusionQueries() && IsValidRef(SceneContext.SmallDepthZ) && IsValidRef(SceneContext.GetSmallDepthSurface());	

	if (bRenderQueries)
	{
		int32 const NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
		
		bool bBatchedQueries = false;
		
		struct FViewOcclusionQueries
		{
			TArray<TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef>> PointLightQueries;
			TArray<TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef>> CSMQueries;
			TArray<TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef>> ShadowQueries;
			TArray<TPair<FPlanarReflectionSceneProxy const*, FRenderQueryRHIRef>> ReflectionQueries;
		};
		
		TArray<FViewOcclusionQueries> ViewQueries;
		ViewQueries.AddDefaulted(Views.Num());

		// Perform occlusion queries for each view
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			FViewOcclusionQueries& ViewQuery = ViewQueries[ViewIndex];
			FSceneViewState* ViewState = (FSceneViewState*)View.State;

			if (ViewState && !View.bDisableQuerySubmissions)
			{
				// Issue this frame's occlusion queries (occlusion queries from last frame may still be in flight)
				const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(ViewState->PendingPrevFrameNumber, NumBufferedFrames);
				FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMaps[QueryIndex];

				// Clear primitives which haven't been visible recently out of the occlusion history, and reset old pending occlusion queries.
				ViewState->TrimOcclusionHistory(RHICmdList, ViewFamily.CurrentRealTime, ViewFamily.CurrentRealTime - GEngine->PrimitiveProbablyVisibleTime, ViewFamily.CurrentRealTime, ViewState->OcclusionFrameCounter);

				// Give back all these occlusion queries to the pool.
				for (TMap<FSceneViewState::FProjectedShadowKey, FRenderQueryRHIRef>::TIterator QueryIt(ShadowOcclusionQueryMap); QueryIt; ++QueryIt)
				{
					//FRenderQueryRHIParamRef Query = QueryIt.Value();
					//check( Query.GetRefCount() == 1 );
					ViewState->OcclusionQueryPool.ReleaseQuery(QueryIt.Value());
				}
				ShadowOcclusionQueryMap.Reset();

				{
					SCOPED_DRAW_EVENT(RHICmdList, ShadowFrustumQueries);

					for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
					{
						const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];

						for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
						{
							const FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.AllProjectedShadows[ShadowIndex];

							if (ProjectedShadowInfo.DependentView && ProjectedShadowInfo.DependentView != &View)
							{
								continue;
							}

							if (!IsShadowCacheModeOcclusionQueryable(ProjectedShadowInfo.CacheMode))
							{
								// Only query one of the cache modes for each shadow
								continue;
							}

							if (ProjectedShadowInfo.bOnePassPointLightShadow)
							{
								FRenderQueryRHIRef ShadowOcclusionQuery;
								if (AllocateProjectedShadowOcclusionQuery(View, ProjectedShadowInfo, NumBufferedFrames, SOQ_LightInfluenceSphere, ShadowOcclusionQuery))
								{
									ViewQuery.PointLightQueries.Add(TPairInitializer<FProjectedShadowInfo const*, FRenderQueryRHIRef>(&ProjectedShadowInfo, ShadowOcclusionQuery));
									bBatchedQueries = true;
								}
							}
							else if (ProjectedShadowInfo.IsWholeSceneDirectionalShadow())
							{
								// Don't query the first cascade, it is always visible
								if (GOcclusionCullCascadedShadowMaps && ProjectedShadowInfo.CascadeSettings.ShadowSplitIndex > 0)
								{
									FRenderQueryRHIRef ShadowOcclusionQuery;
									if (AllocateProjectedShadowOcclusionQuery(View, ProjectedShadowInfo, NumBufferedFrames, SOQ_None, ShadowOcclusionQuery))
									{
										ViewQuery.CSMQueries.Add(TPairInitializer<FProjectedShadowInfo const*, FRenderQueryRHIRef>(&ProjectedShadowInfo, ShadowOcclusionQuery));
										bBatchedQueries = true;
									}
								}
							}
							else if (
							// Don't query preshadows, since they are culled if their subject is occluded.
								!ProjectedShadowInfo.bPreShadow
							// Don't query if any subjects are visible because the shadow frustum will be definitely unoccluded
								&& !ProjectedShadowInfo.SubjectsVisible(View))
							{
								FRenderQueryRHIRef ShadowOcclusionQuery;
								if (AllocateProjectedShadowOcclusionQuery(View, ProjectedShadowInfo, NumBufferedFrames, SOQ_NearPlaneVsShadowFrustum, ShadowOcclusionQuery))
								{
									ViewQuery.ShadowQueries.Add(TPairInitializer<FProjectedShadowInfo const*, FRenderQueryRHIRef>(&ProjectedShadowInfo, ShadowOcclusionQuery));
									bBatchedQueries = true;
								}
							}
						}

						// Issue occlusion queries for all per-object projected shadows that we would have rendered but were occluded last frame.
						for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.OccludedPerObjectShadows.Num(); ShadowIndex++)
						{
							const FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.OccludedPerObjectShadows[ShadowIndex];
							FRenderQueryRHIRef ShadowOcclusionQuery;
							if (AllocateProjectedShadowOcclusionQuery(View, ProjectedShadowInfo, NumBufferedFrames, SOQ_NearPlaneVsShadowFrustum, ShadowOcclusionQuery))
							{
								ViewQuery.ShadowQueries.Add(TPairInitializer<FProjectedShadowInfo const*, FRenderQueryRHIRef>(&ProjectedShadowInfo, ShadowOcclusionQuery));
								bBatchedQueries = true;
							}
						}
					}
				}

				if (!View.bIsPlanarReflection &&
					!View.bIsSceneCapture &&
					!View.bIsReflectionCapture)
				{
					// +1 to buffered frames because the query is submitted late into the main frame, but read at the beginning of a frame
					const int32 NumReflectionBufferedFrames = NumBufferedFrames + 1;

					for (int32 ReflectionIndex = 0; ReflectionIndex < Scene->PlanarReflections.Num(); ReflectionIndex++)
					{
						FPlanarReflectionSceneProxy* SceneProxy = Scene->PlanarReflections[ReflectionIndex];
						FRenderQueryRHIRef ShadowOcclusionQuery;
						if (AllocatePlanarReflectionOcclusionQuery(View, SceneProxy, NumBufferedFrames, ShadowOcclusionQuery))
						{
							ViewQuery.ReflectionQueries.Add(TPairInitializer<FPlanarReflectionSceneProxy const*, FRenderQueryRHIRef>(SceneProxy, ShadowOcclusionQuery));
							bBatchedQueries = true;
						}
					}
				}

				bBatchedQueries |= (View.IndividualOcclusionQueries.HasBatches() || View.GroupedOcclusionQueries.HasBatches()
									// Don't do primitive occlusion if we have a view parent or are frozen - only applicable to Debug & Development.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
									|| (!ViewState->HasViewParent() && !ViewState->bIsFrozen)
#endif
									);
			}
		}

		// Don't do anything if we have no queries batched.
		if (bBatchedQueries)
		{
			SCOPED_DRAW_EVENT(RHICmdList, BeginOcclusionTests);
			
			if (bUseDownsampledDepth)
			{
				SetRenderTarget(RHICmdList, NULL, SceneContext.GetSmallDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
			}
			else
			{
				SetRenderTarget(RHICmdList, NULL, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
			}
			
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
			// Depth tests, no depth writes, no color writes, opaque
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
			
			RHICmdList.BeginOcclusionQueryBatch();
			
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_DRAW_EVENTF(RHICmdList, ViewOcclusionTests, TEXT("ViewOcclusionTests %d"), ViewIndex);
				
				FViewInfo& View = Views[ViewIndex];
				FViewOcclusionQueries& ViewQuery = ViewQueries[ViewIndex];
				FSceneViewState* ViewState = (FSceneViewState*)View.State;
				
				// We only need to render the front-faces of the culling geometry (this halves the amount of pixels we touch)
				GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
				
				if (bUseDownsampledDepth)
				{
					const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X / SceneContext.GetSmallColorDepthDownsampleFactor());
					const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y / SceneContext.GetSmallColorDepthDownsampleFactor());
					const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() / SceneContext.GetSmallColorDepthDownsampleFactor());
					const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() / SceneContext.GetSmallColorDepthDownsampleFactor());
					
					// Setup the viewport for rendering to the downsampled depth buffer
					RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);
				}
				else
				{
					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
				}
				
				// Lookup the vertex shader.
				TShaderMapRef<FOcclusionQueryVS> VertexShader(View.ShaderMap);
				
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector3();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				VertexShader->SetParameters(RHICmdList, View);
				
				{
					SCOPED_DRAW_EVENT(RHICmdList, ShadowFrustumQueries);
					for (TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef> const& Query : ViewQuery.PointLightQueries)
					{
						ExecutePointLightShadowOcclusionQuery(RHICmdList, View, *Query.Key, *VertexShader, Query.Value);
					}

					for (TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef> const& Query : ViewQuery.CSMQueries)
					{
						ExecuteDirectionalLightShadowOcclusionQuery(RHICmdList, View, *Query.Key, *VertexShader, Query.Value);
					}
					
					for (TPair<FProjectedShadowInfo const*, FRenderQueryRHIRef> const& Query : ViewQuery.ShadowQueries)
					{
						ExecuteProjectedShadowOcclusionQuery(RHICmdList, View, *Query.Key, *VertexShader, Query.Value);
					}
				}
				
				{
					SCOPED_DRAW_EVENT(RHICmdList, PlanarReflectionQueries);
					for (TPair<FPlanarReflectionSceneProxy const*, FRenderQueryRHIRef> const& Query : ViewQuery.ReflectionQueries)
					{
						ExecutePlanarReflectionOcclusionQuery(RHICmdList, View, Query.Key, *VertexShader, Query.Value);
					}
				}
				
				// Don't do primitive occlusion if we have a view parent or are frozen - only applicable to Debug & Development.
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if(!ViewState->HasViewParent() && !ViewState->bIsFrozen)
	#endif
				{
					VertexShader->SetParameters(RHICmdList, View);
					
					{
						SCOPED_DRAW_EVENT(RHICmdList, IndividualQueries);
						View.IndividualOcclusionQueries.Flush(RHICmdList);
					}
					{
						SCOPED_DRAW_EVENT(RHICmdList, GroupedQueries);
						View.GroupedOcclusionQueries.Flush(RHICmdList);
					}
				}
			}
			
			RHICmdList.EndOcclusionQueryBatch();
			
			if (bUseDownsampledDepth)
			{
				// Restore default render target
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
			}
		}
	}
}
