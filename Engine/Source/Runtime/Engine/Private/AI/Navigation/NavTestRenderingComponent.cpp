// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavTestRenderingComponent.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "SceneManagement.h"
#include "AI/Navigation/NavigationTestingActor.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "Debug/DebugDrawService.h"

static const FColor NavMeshRenderColor_OpenSet(255,128,0,255);
static const FColor NavMeshRenderColor_ClosedSet(255,196,0,255);
static const uint8 NavMeshRenderAlpha_Modifed = 255;
static const uint8 NavMeshRenderAlpha_NonModified = 64;

FNavTestSceneProxy::FNavTestSceneProxy(const UNavTestRenderingComponent* InComponent)
	: FDebugRenderSceneProxy(InComponent)
	, NavMeshDrawOffset(0,0,10)
	, NavTestActor(NULL)
{
	if (InComponent == NULL)
	{
		return;
	}

	NavTestActor = Cast<ANavigationTestingActor>(InComponent->GetOwner());
	if (NavTestActor == NULL)
	{
		return;
	}

	NavMeshDrawOffset.Z += NavTestActor->NavAgentProps.AgentRadius / 10.f;
	bShowNodePool = NavTestActor->bShowNodePool;
	bShowBestPath = NavTestActor->bShowBestPath;
	bShowDiff = NavTestActor->bShowDiffWithPreviousStep;

	ClosestWallLocation = NavTestActor->bDrawDistanceToWall ? NavTestActor->ClosestWallLocation : FNavigationSystem::InvalidLocation;

	GatherPathPoints();
	GatherPathStep();
}

void FNavTestSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			if (NavTestActor)
			{
				//DrawArc(PDI, Link.Left, Link.Right, 0.4f, NavMeshColors[Link.AreaID], SDPG_World, 3.5f);
				//const FVector VOffset(0,0,FVector::Dist(Link.Left, Link.Right)*1.333f);
				//DrawArrowHead(PDI, Link.Right, Link.Left+VOffset, 30.f, NavMeshColors[Link.AreaID], SDPG_World, 3.5f);

				//@todo - the rendering thread should never read from UObjects directly!  These are race conditions, the properties should be mirrored on the proxy
				const FVector ActorLocation = NavTestActor->GetActorLocation();
				const FVector ProjectedLocation = NavTestActor->ProjectedLocation + NavMeshDrawOffset;
				const FColor ProjectedColor = NavTestActor->bProjectedLocationValid ? FColor(0, 255, 0, 120) : FColor(255, 0, 0, 120);
				const FColor ClosestWallColor = FColorList::Orange;
				const FVector BoxExtent(20, 20, 20);

				FMaterialRenderProxy* const ColoredMeshInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy(GEngine->DebugMeshMaterial->GetRenderProxy(false), ProjectedColor);
				//DrawBox(PDI, FTransform(ProjectedLocation).ToMatrixNoScale(),BoxExtent, ColoredMeshInstance, SDPG_World);
				GetSphereMesh(ProjectedLocation, BoxExtent, 10, 7, ColoredMeshInstance, SDPG_World, false, ViewIndex, Collector);

				//DrawWireBox(PDI, FBox(ProjectedLocation-BoxExtent, ProjectedLocation+BoxExtent), ProjectedColor, false);
				DrawWireBox(PDI, FBox(ActorLocation - BoxExtent, ActorLocation + BoxExtent), FColor::White, false);
				const FVector LineEnd = ProjectedLocation - (ProjectedLocation - ActorLocation).GetSafeNormal()*BoxExtent.X;
				PDI->DrawLine(LineEnd, ActorLocation, ProjectedColor, SDPG_World, 2.5);
				DrawArrowHead(PDI, LineEnd, ActorLocation, 20.f, ProjectedColor, SDPG_World, 2.5f);

				// draw query extent
				DrawWireBox(PDI, FBox(ActorLocation - NavTestActor->QueryingExtent, ActorLocation + NavTestActor->QueryingExtent), FColor::Blue, false);

				if (FNavigationSystem::IsValidLocation(ClosestWallLocation))
				{
					PDI->DrawLine(ClosestWallLocation, ActorLocation, ClosestWallColor, SDPG_World, 2.5);
				}
			}

			// draw path
			if (!bShowBestPath || !NodeDebug.Num())
			{
				for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); PointIndex++)
				{
					PDI->DrawLine(PathPoints[PointIndex-1], PathPoints[PointIndex], FLinearColor::Red, SDPG_World, 2.0f, 0.0f, true);
				}
			}

			// draw path debug data
			if (bShowNodePool)
			{
				if (ClosedSetIndices.Num())
				{
					const FColoredMaterialRenderProxy *MeshColorInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy(GEngine->DebugMeshMaterial->GetRenderProxy(false), NavMeshRenderColor_ClosedSet);						
					FDynamicMeshBuilder	MeshBuilder;
					MeshBuilder.AddVertices(ClosedSetVerts);
					MeshBuilder.AddTriangles(ClosedSetIndices);
					MeshBuilder.GetMesh(FMatrix::Identity, MeshColorInstance, GetDepthPriorityGroup(View), false, false, ViewIndex, Collector);
				}

				if (OpenSetIndices.Num())
				{
					const FColoredMaterialRenderProxy *MeshColorInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy(GEngine->DebugMeshMaterial->GetRenderProxy(false), NavMeshRenderColor_OpenSet);						
					FDynamicMeshBuilder	MeshBuilder;
					MeshBuilder.AddVertices(OpenSetVerts);
					MeshBuilder.AddTriangles(OpenSetIndices);
					MeshBuilder.GetMesh(FMatrix::Identity, MeshColorInstance, GetDepthPriorityGroup(View), false, false, ViewIndex, Collector);
				}
			}

			for (TSet<FNodeDebugData>::TConstIterator It(NodeDebug); It; ++It)
			{
				const FNodeDebugData& NodeData = *It;

				FColor LineColor(FColor::Blue);
				if (bShowBestPath && NodeData.bBestPath)
				{
					LineColor = FColor::Red;
				}

				if (bShowDiff)
				{
					LineColor.A = NodeData.bModified ? NavMeshRenderAlpha_Modifed : NavMeshRenderAlpha_NonModified;
				}

				FVector ParentPos(NodeData.ParentId.IsValidId() ? NodeDebug[NodeData.ParentId].Position : NodeData.Position);

				if (bShowDiff && !NodeData.bModified)
				{
					PDI->DrawLine(NodeData.Position, ParentPos, LineColor, SDPG_World);
				}
				else
				{
					PDI->DrawLine(NodeData.Position, ParentPos, LineColor, SDPG_World, 2.0f, 0.0, true);
				}

				if (NodeData.bOffMeshLink)
				{
					DrawWireBox(PDI, FBox::BuildAABB(NodeData.Position, FVector(10.0f)), LineColor, SDPG_World);
				}

				if (bShowDiff && NodeData.bModified)
				{
					PDI->DrawLine(NodeData.Position + FVector(0,0,10), NodeData.Position + FVector(0,0,100), FColor::Green, SDPG_World);
				}
			}
		}
	}
}

void FNavTestSceneProxy::GatherPathPoints()
{
	if (NavTestActor && NavTestActor->LastPath.IsValid())
	{
		for (int32 PointIndex = 0; PointIndex < NavTestActor->LastPath->GetPathPoints().Num(); PointIndex++)
		{
			PathPoints.Add(NavTestActor->LastPath->GetPathPoints()[PointIndex].Location);
			PathPointFlags.Add(FString::Printf(TEXT("%d-%d"), PointIndex, FNavMeshNodeFlags(NavTestActor->LastPath->GetPathPoints()[PointIndex].Flags).AreaFlags));
		}
	}
}

void FNavTestSceneProxy::GatherPathStep()
{
	OpenSetVerts.Reset();
	ClosedSetVerts.Reset();
	OpenSetIndices.Reset();
	ClosedSetIndices.Reset();
	NodeDebug.Empty(NodeDebug.Num());
	BestNodeId = FSetElementId();

#if WITH_EDITORONLY_DATA && WITH_RECAST
	if (NavTestActor && NavTestActor->DebugSteps.Num() && NavTestActor->ShowStepIndex >= 0)
	{
		const int32 ShowIdx = FMath::Min(NavTestActor->ShowStepIndex, NavTestActor->DebugSteps.Num() - 1);
		const FRecastDebugPathfindingData& DebugStep = NavTestActor->DebugSteps[ShowIdx];
		int32 BaseOpen = 0;
		int32 BaseClosed = 0;

		for (TSet<FRecastDebugPathfindingNode>::TConstIterator It(DebugStep.Nodes); It; ++It)
		{
			const FRecastDebugPathfindingNode& DebugNode = *It;
			if (DebugNode.bOpenSet)
			{
				for (int32 iv = 0; iv < DebugNode.Verts.Num(); iv++)
				{
					OpenSetVerts.Add(DebugNode.Verts[iv] + NavMeshDrawOffset);
				}

				for (int32 iv = 2; iv < DebugNode.Verts.Num(); iv++)
				{
					OpenSetIndices.Add(BaseOpen + 0);
					OpenSetIndices.Add(BaseOpen + iv - 1);
					OpenSetIndices.Add(BaseOpen + iv);
				}

				BaseOpen += DebugNode.Verts.Num();
			}
			else
			{
				for (int32 iv = 0; iv < DebugNode.Verts.Num(); iv++)
				{
					ClosedSetVerts.Add(DebugNode.Verts[iv] + NavMeshDrawOffset);
				}

				for (int32 iv = 2; iv < DebugNode.Verts.Num(); iv++)
				{
					ClosedSetIndices.Add(BaseClosed + 0);
					ClosedSetIndices.Add(BaseClosed + iv - 1);
					ClosedSetIndices.Add(BaseClosed + iv);
				}

				BaseClosed += DebugNode.Verts.Num();
			}

			FNodeDebugData NewNodeData;

			float DisplayedCost = FLT_MAX; 
			switch (NavTestActor->CostDisplayMode)
			{
			case ENavCostDisplay::TotalCost:
				DisplayedCost = DebugNode.TotalCost;
				break;
			case ENavCostDisplay::RealCostOnly:
				DisplayedCost = DebugNode.Cost;
				break;
			case ENavCostDisplay::HeuristicOnly:
				DisplayedCost = DebugNode.GetHeuristicCost();
				break;
			default:
				break;
			}

			NewNodeData.Desc = FString::Printf(TEXT("%.2f%s"), DisplayedCost, DebugNode.bOffMeshLink ? TEXT(" [link]") : TEXT(""));

			NewNodeData.Position = DebugNode.NodePos;
			NewNodeData.PolyRef = DebugNode.PolyRef;
			NewNodeData.bClosedSet = !DebugNode.bOpenSet;
			NewNodeData.bBestPath = (It.GetId() == DebugStep.BestNode);
			NewNodeData.bModified = DebugNode.bModified;
			NewNodeData.bOffMeshLink = DebugNode.bOffMeshLink;

			const FSetElementId NewId = NodeDebug.Add(NewNodeData);
			if (NewNodeData.bBestPath)
			{
				BestNodeId = NewId;
			}
		}

		FRecastDebugPathfindingNode ThisNode;
		FNodeDebugData ParentDebugNode;

		for (TSet<FNodeDebugData>::TIterator It(NodeDebug); It; ++It)
		{
			FNodeDebugData& MyDebugNode = *It;
				
			ThisNode.PolyRef = MyDebugNode.PolyRef;
			const FRecastDebugPathfindingNode* MyNode = DebugStep.Nodes.Find(ThisNode);

			if (MyNode)
			{
				ParentDebugNode.PolyRef = MyNode->ParentRef;
				MyDebugNode.ParentId = NodeDebug.FindId(ParentDebugNode);
			}
		}

		FSetElementId BestPathId = BestNodeId;
		while (BestPathId.IsValidId())
		{
			FNodeDebugData& MyDebugNode = NodeDebug[BestPathId];

			MyDebugNode.bBestPath = true;
			BestPathId = MyDebugNode.ParentId;
		}
	}
#endif
}

FPrimitiveViewRelevance FNavTestSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bDynamicRelevance = true;
	// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
	Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = IsShown(View) && GIsEditor;
	return Result;
}

uint32 FNavTestSceneProxy::GetAllocatedSize(void) const
{
	int32 InternalAllocSize = 0;
	for (TSet<FNodeDebugData>::TConstIterator It(NodeDebug); It; ++It)
	{
		InternalAllocSize += (*It).Desc.GetAllocatedSize();
	}

	return FDebugRenderSceneProxy::GetAllocatedSize() + PathPoints.GetAllocatedSize()
		+ PathPointFlags.GetAllocatedSize()
		+ OpenSetVerts.GetAllocatedSize() + OpenSetIndices.GetAllocatedSize()
		+ ClosedSetVerts.GetAllocatedSize() + ClosedSetIndices.GetAllocatedSize()
		+ NodeDebug.GetAllocatedSize() + InternalAllocSize;

}

#if WITH_RECAST && WITH_EDITOR
void FNavTestDebugDrawDelegateHelper::InitDelegateHelper(const FNavTestSceneProxy* InSceneProxy)
{
	Super::InitDelegateHelper(InSceneProxy);

	PathPoints.Reset();
	PathPoints.Append(InSceneProxy->PathPoints);
	PathPointFlags.Reset();
	PathPointFlags.Append(InSceneProxy->PathPointFlags);
	NodeDebug.Reset();
	NodeDebug.Append(InSceneProxy->NodeDebug);
	NavTestActor = InSceneProxy->NavTestActor;
	BestNodeId = InSceneProxy->BestNodeId;
	bShowBestPath = InSceneProxy->bShowBestPath;
	bShowDiff = InSceneProxy->bShowDiff;
}

void FNavTestDebugDrawDelegateHelper::RegisterDebugDrawDelgate()
{
	ensureMsgf(State != RegisteredState, TEXT("RegisterDebugDrawDelgate is already Registered!"));
	if (State == InitializedState)
	{
		DebugTextDrawingDelegate = FDebugDrawDelegate::CreateRaw(this, &FNavTestDebugDrawDelegateHelper::DrawDebugLabels);
		DebugTextDrawingDelegateHandle = UDebugDrawService::Register(TEXT("Navigation"), DebugTextDrawingDelegate);
		State = RegisteredState;
	}
}

void FNavTestDebugDrawDelegateHelper::UnregisterDebugDrawDelgate()
{
	ensureMsgf(State != InitializedState, TEXT("UnegisterDebugDrawDelgate is in an invalid State: %i !"), State);
	if (State == RegisteredState)
	{
		check(DebugTextDrawingDelegate.IsBound());
		UDebugDrawService::Unregister(DebugTextDrawingDelegateHandle);
		State = InitializedState;
	}
}

void FNavTestDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController*)
{
	if (NavTestActor == NULL)
	{
		return;
	}

	const FColor OldDrawColor = Canvas->DrawColor;
	Canvas->SetDrawColor(FColor::White);
	const FSceneView* View = Canvas->SceneView;

#if WITH_EDITORONLY_DATA && WITH_RECAST
	if (NodeDebug.Num())
	{
		UFont* RenderFont = GEngine->GetSmallFont();
		for (TSet<FNavTestSceneProxy::FNodeDebugData>::TConstIterator It(NodeDebug); It; ++It)
		{
			const FNavTestSceneProxy::FNodeDebugData& NodeData = *It;

			if (FNavTestSceneProxy::LocationInView(NodeData.Position, View))
			{
				FColor MyColor = NodeData.bClosedSet ? FColor(64, 64, 64) : FColor::White;
				if (!bShowBestPath && It.GetId() == BestNodeId)
				{
					MyColor = FColor::Red;
				}
				if (bShowDiff)
				{
					MyColor.A = NodeData.bModified ? NavMeshRenderAlpha_Modifed : NavMeshRenderAlpha_NonModified;
				}

				Canvas->SetDrawColor(MyColor);

				const FVector ScreenLoc = Canvas->Project(NodeData.Position) + FVector(NavTestActor->TextCanvasOffset, 0.f);
				Canvas->DrawText(RenderFont, NodeData.Desc, ScreenLoc.X, ScreenLoc.Y);
			}
		}
	}
	else
	{
#endif
		for (int32 PointIndex = 0; PointIndex < PathPoints.Num(); ++PointIndex)
		{
			if (FNavTestSceneProxy::LocationInView(PathPoints[PointIndex], View))
			{
				const FVector PathPointLoc = Canvas->Project(PathPoints[PointIndex]);
				UFont* RenderFont = GEngine->GetSmallFont();
				Canvas->DrawText(RenderFont, PathPointFlags[PointIndex], PathPointLoc.X, PathPointLoc.Y);
			}
		}

#if WITH_EDITORONLY_DATA && WITH_RECAST
	}
#endif
	Canvas->SetDrawColor(OldDrawColor);
}
#endif //WITH_RECAST && WITH_EDITOR

UNavTestRenderingComponent::UNavTestRenderingComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FPrimitiveSceneProxy* UNavTestRenderingComponent::CreateSceneProxy()
{
	FNavTestSceneProxy* SceneProxy2 = new FNavTestSceneProxy(this);
#if WITH_RECAST && WITH_EDITOR
	NavTestDebugDrawDelegateHelper.InitDelegateHelper(SceneProxy2);
	NavTestDebugDrawDelegateHelper.ReregisterDebugDrawDelgate();
#endif
	return SceneProxy2;
}

FBoxSphereBounds UNavTestRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	ANavigationTestingActor* TestActor = Cast<ANavigationTestingActor>(GetOwner());
	if (TestActor)
	{
		BoundingBox = TestActor->GetComponentsBoundingBox(true);
	
		if (TestActor->LastPath.IsValid())
		{
			for (int32 PointIndex = 0; PointIndex < TestActor->LastPath->GetPathPoints().Num(); PointIndex++)
			{
				BoundingBox += TestActor->LastPath->GetPathPoints()[PointIndex].Location;
			}
		}
#if WITH_EDITORONLY_DATA && WITH_RECAST
		if (TestActor->DebugSteps.Num() && TestActor->ShowStepIndex >= 0)
		{
			const int32 ShowIdx = FMath::Min(TestActor->ShowStepIndex, TestActor->DebugSteps.Num() - 1);
			const FRecastDebugPathfindingData& DebugStep = TestActor->DebugSteps[ShowIdx];
			for (TSet<FRecastDebugPathfindingNode>::TConstIterator It(DebugStep.Nodes); It; ++It)
			{
				const FRecastDebugPathfindingNode& DebugNode = *It;
				for (int32 iv = 0; iv < DebugNode.Verts.Num(); iv++)
				{
					BoundingBox += DebugNode.Verts[iv];
				}
			}
		}
#endif
	}

	return FBoxSphereBounds(BoundingBox);
}

void UNavTestRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_RECAST && WITH_EDITOR
	NavTestDebugDrawDelegateHelper.RegisterDebugDrawDelgate();
#endif
}

void UNavTestRenderingComponent::DestroyRenderState_Concurrent()
{
#if WITH_RECAST && WITH_EDITOR
	NavTestDebugDrawDelegateHelper.UnregisterDebugDrawDelgate();
#endif

	Super::DestroyRenderState_Concurrent();
}
