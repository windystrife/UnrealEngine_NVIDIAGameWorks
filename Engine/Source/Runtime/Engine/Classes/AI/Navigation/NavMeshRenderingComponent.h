// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "MaterialShared.h"
#include "DynamicMeshBuilder.h"
#include "DebugRenderSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "MeshBatch.h"
#include "LocalVertexFactory.h"
#include "GenericOctree.h"
#include "NavMeshRenderingComponent.generated.h"

class APlayerController;
class ARecastNavMesh;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class UCanvas;
class UNavMeshRenderingComponent;

enum class ENavMeshDetailFlags : uint8
{
	TriangleEdges,
	PolyEdges,
	BoundaryEdges,
	FilledPolys,
	TileBounds,
	PathCollidingGeometry,
	TileLabels,
	PolygonLabels,
	PolygonCost,
	PathLabels,
	NavLinks,
	FailedNavLinks,
	Clusters,
	NavOctree,
};

// exported to API for GameplayDebugger module
struct ENGINE_API FNavMeshSceneProxyData : public TSharedFromThis<FNavMeshSceneProxyData, ESPMode::ThreadSafe>
{
	struct FDebugMeshData
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		FColor ClusterColor;
	};
	TArray<FDebugMeshData> MeshBuilders;

	TArray<FDebugRenderSceneProxy::FDebugLine> ThickLineItems;
	TArray<FDebugRenderSceneProxy::FDebugLine> TileEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavMeshEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavLinkLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> ClusterLinkLines;

	struct FDebugText
	{
		FVector Location;
		FString Text;

		FDebugText() {}
		FDebugText(const FVector& InLocation, const FString& InText) : Location(InLocation), Text(InText) {}
	};
	TArray<FDebugText> DebugLabels;
	
	TArray<int32> PathCollidingGeomIndices;
	TArray<FDynamicMeshVertex> PathCollidingGeomVerts;
	TArray<FBoxCenterAndExtent>	OctreeBounds;

	FBox Bounds;
	FVector NavMeshDrawOffset;
	uint32 bDataGathered : 1;
	uint32 bNeedsNewData : 1;
	int32 NavDetailFlags;

	FNavMeshSceneProxyData() : NavMeshDrawOffset(0, 0, 10.f),
		bDataGathered(false), bNeedsNewData(true), NavDetailFlags(0) {}

	void Reset();
	void Serialize(FArchive& Ar);
	uint32 GetAllocatedSize() const;

#if WITH_RECAST
	int32 GetDetailFlags(const ARecastNavMesh* NavMesh) const;
	void GatherData(const ARecastNavMesh* NavMesh, int32 InNavDetailFlags, const TArray<int32>& TileSet);
#endif
};

// exported to API for GameplayDebugger module
class ENGINE_API FNavMeshSceneProxy : public FDebugRenderSceneProxy
{
	friend class FNavMeshDebugDrawDelegateHelper;
public:
	FNavMeshSceneProxy(const UPrimitiveComponent* InComponent, FNavMeshSceneProxyData* InProxyData, bool ForceToRender = false);
	virtual ~FNavMeshSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

protected:
	void DrawDebugBox(FPrimitiveDrawInterface* PDI, FVector const& Center, FVector const& Box, FColor const& Color) const;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }
	uint32 GetAllocatedSize(void) const;

private:
	class FNavMeshIndexBuffer : public FIndexBuffer
	{
	public:
		TArray<int32> Indices;
		virtual void InitRHI() override;
	};

	class FNavMeshVertexBuffer : public FVertexBuffer
	{
	public:
		TArray<FDynamicMeshVertex> Vertices;
		virtual void InitRHI() override;
	};

	class FNavMeshVertexFactory : public FLocalVertexFactory
	{
	public:
		void Init(const FNavMeshVertexBuffer* VertexBuffer);
	};
			
	FNavMeshSceneProxyData ProxyData;

	FNavMeshIndexBuffer IndexBuffer;
	FNavMeshVertexBuffer VertexBuffer;
	FNavMeshVertexFactory VertexFactory;

	TArray<FColoredMaterialRenderProxy> MeshColors;
	TArray<FMeshBatchElement> MeshBatchElements;

	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	TWeakObjectPtr<UNavMeshRenderingComponent> RenderingComponent;
	uint32 bRequestedData : 1;
	uint32 bForceRendering : 1;
	uint32 bSkipDistanceCheck : 1;
	uint32 bUseThickLines : 1;
};

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
class FNavMeshDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FNavMeshDebugDrawDelegateHelper()
		: bForceRendering(false)
		, bNeedsNewData(false)
	{
	}

	virtual void InitDelegateHelper(const FDebugRenderSceneProxy* InSceneProxy) override
	{
		check(0);
	}

	void InitDelegateHelper(const FNavMeshSceneProxy* InSceneProxy)
	{
		Super::InitDelegateHelper(InSceneProxy);

		DebugLabels.Reset();
		DebugLabels.Append(InSceneProxy->ProxyData.DebugLabels);
		bForceRendering = InSceneProxy->bForceRendering;
		bNeedsNewData = InSceneProxy->ProxyData.bNeedsNewData;
	}

	ENGINE_API virtual void RegisterDebugDrawDelgate() override;
	ENGINE_API virtual void UnregisterDebugDrawDelgate() override;

protected:
	ENGINE_API virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;

private:
	TArray<FNavMeshSceneProxyData::FDebugText> DebugLabels;
	uint32 bForceRendering : 1;
	uint32 bNeedsNewData : 1;
};
#endif

UCLASS(hidecategories=Object, editinlinenew)
class ENGINE_API UNavMeshRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	
	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;
	//~ End UPrimitiveComponent Interface

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	//~ End USceneComponent Interface

	void ForceUpdate() { bForceUpdate = true; }
	bool IsForcingUpdate() const { return bForceUpdate; }

	static bool IsNavigationShowFlagSet(const UWorld* World);

protected:
	void TimerFunction();

protected:
	uint32 bCollectNavigationData : 1;
	uint32 bForceUpdate : 1;
	FTimerHandle TimerHandle;

protected:
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FNavMeshDebugDrawDelegateHelper NavMeshDebugDrawDelgateManager;
#endif
};

namespace FNavMeshRenderingHelpers
{
	ENGINE_API void AddVertex(FNavMeshSceneProxyData::FDebugMeshData& MeshData, const FVector& Pos, const FColor Color = FColor::White);

	ENGINE_API void AddTriangle(FNavMeshSceneProxyData::FDebugMeshData& MeshData, int32 V0, int32 V1, int32 V2);
}
