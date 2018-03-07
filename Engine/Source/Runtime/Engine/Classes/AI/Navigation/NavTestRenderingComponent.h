// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PrimitiveViewRelevance.h"
#include "Components/PrimitiveComponent.h"
#include "DynamicMeshBuilder.h"
#include "DebugRenderSceneProxy.h"

class ANavigationTestingActor;
class APlayerController;
class FMeshElementCollector;
class UCanvas;
class UNavTestRenderingComponent;
struct FNodeDebugData;

class FNavTestSceneProxy : public FDebugRenderSceneProxy
{
	friend class FNavTestDebugDrawDelegateHelper;

public:

	struct FNodeDebugData
	{
		NavNodeRef PolyRef;
		FVector Position;
		FString Desc;
		FSetElementId ParentId;
		uint32 bClosedSet : 1;
		uint32 bBestPath : 1;
		uint32 bModified : 1;
		uint32 bOffMeshLink : 1;

		FORCEINLINE bool operator==(const FNodeDebugData& Other) const
		{
			return PolyRef == Other.PolyRef;
		}
		FORCEINLINE friend uint32 GetTypeHash(const FNodeDebugData& Other)
		{
			return Other.PolyRef;
		}
	};

	FNavTestSceneProxy(const UNavTestRenderingComponent* InComponent);

	~FNavTestSceneProxy() {}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	void GatherPathPoints();

	void GatherPathStep();

	FORCEINLINE static bool LocationInView(const FVector& Location, const FSceneView* View)
	{
		return View->ViewFrustum.IntersectBox(Location, FVector::ZeroVector);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }

	uint32 GetAllocatedSize(void) const;

private:
	FVector NavMeshDrawOffset;
	ANavigationTestingActor* NavTestActor;
	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	TArray<FVector> PathPoints;
	TArray<FString> PathPointFlags;

	TArray<FDynamicMeshVertex> OpenSetVerts;
	TArray<int32> OpenSetIndices;
	TArray<FDynamicMeshVertex> ClosedSetVerts;
	TArray<int32> ClosedSetIndices;
	TSet<FNodeDebugData> NodeDebug;
	FSetElementId BestNodeId;

	FVector ClosestWallLocation;

	uint32 bShowBestPath : 1;
	uint32 bShowNodePool : 1;
	uint32 bShowDiff : 1;
};

#if WITH_RECAST && WITH_EDITOR
class FNavTestDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	virtual void InitDelegateHelper(const FDebugRenderSceneProxy* InSceneProxy) override
	{
		check(0);
	}

	void InitDelegateHelper(const FNavTestSceneProxy* InSceneProxy);

	virtual void RegisterDebugDrawDelgate() override;
	virtual void UnregisterDebugDrawDelgate() override;

protected:
	void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;

private:
	TSet<FNavTestSceneProxy::FNodeDebugData> NodeDebug;
	ANavigationTestingActor* NavTestActor;
	TArray<FVector> PathPoints;
	TArray<FString> PathPointFlags;
	FSetElementId BestNodeId;
	uint32 bShowBestPath : 1;
	uint32 bShowDiff : 1;
};
#endif //WITH_RECAST && WITH_EDITOR

#include "NavTestRenderingComponent.generated.h"

class FPrimitiveSceneProxy;
struct FTransform;

UCLASS(hidecategories=Object)
class UNavTestRenderingComponent: public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;

private:
#if WITH_RECAST && WITH_EDITOR
	FNavTestDebugDrawDelegateHelper NavTestDebugDrawDelegateHelper;
#endif
};
