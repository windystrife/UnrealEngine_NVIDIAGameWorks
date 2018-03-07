// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavLinkRenderingComponent.h"
#include "EngineGlobals.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Engine.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/NavLinkRenderingProxy.h"
#include "AI/Navigation/NavLinkHostInterface.h"
#include "AI/Navigation/RecastNavMesh.h"

//----------------------------------------------------------------------//
// UNavLinkRenderingComponent
//----------------------------------------------------------------------//
UNavLinkRenderingComponent::UNavLinkRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// properties

	// Allows updating in game, while optimizing rendering for the case that it is not modified
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bIsEditorOnly = true;

	bGenerateOverlapEvents = false;
}

FBoxSphereBounds UNavLinkRenderingComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	AActor* LinkOwnerActor = GetOwner();
	INavLinkHostInterface* LinkOwnerHost = Cast<INavLinkHostInterface>(GetOwner());

	if (LinkOwnerActor != NULL && LinkOwnerHost != NULL)
	{
		FBox BoundingBox(ForceInit);
		const FTransform LocalToWorld = LinkOwnerActor->ActorToWorld();
		TArray<TSubclassOf<UNavLinkDefinition> > NavLinkClasses;
		TArray<FNavigationLink> SimpleLinks;
		TArray<FNavigationSegmentLink> DummySegmentLinks;

		if (LinkOwnerHost->GetNavigationLinksClasses(NavLinkClasses))
		{
			for (int32 NavLinkClassIdx = 0; NavLinkClassIdx < NavLinkClasses.Num(); ++NavLinkClassIdx)
			{
				if (NavLinkClasses[NavLinkClassIdx] != NULL)
				{
					const TArray<FNavigationLink>& Links = UNavLinkDefinition::GetLinksDefinition(NavLinkClasses[NavLinkClassIdx]);
					for (const auto& Link : Links)
					{
						BoundingBox += Link.Left;
						BoundingBox += Link.Right;
					}
				}
			}
		}
		if (LinkOwnerHost->GetNavigationLinksArray(SimpleLinks, DummySegmentLinks))
		{
			for (const auto& Link : SimpleLinks)
			{
				BoundingBox += Link.Left;
				BoundingBox += Link.Right;
			}
		}

		return FBoxSphereBounds(BoundingBox).TransformBy(LocalToWorld);
	}

	return FBoxSphereBounds(EForceInit::ForceInitToZero);
}

FPrimitiveSceneProxy* UNavLinkRenderingComponent::CreateSceneProxy()
{
	return new FNavLinkRenderingProxy(this);
}

#if WITH_EDITOR
bool UNavLinkRenderingComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// NavLink rendering components not treated as 'selectable' in editor
	return false;
}

bool UNavLinkRenderingComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// NavLink rendering components not treated as 'selectable' in editor
	return false;
}
#endif

//----------------------------------------------------------------------//
// FNavLinkRenderingProxy
//----------------------------------------------------------------------//
FNavLinkRenderingProxy::FNavLinkRenderingProxy(const UPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	LinkOwnerActor = InComponent->GetOwner();
	LinkOwnerHost = Cast<INavLinkHostInterface>(InComponent->GetOwner());

	if (LinkOwnerActor != NULL && LinkOwnerHost != NULL)
	{
		const FTransform LinkOwnerLocalToWorld = LinkOwnerActor->ActorToWorld();
		TArray<TSubclassOf<UNavLinkDefinition> > NavLinkClasses;
		LinkOwnerHost->GetNavigationLinksClasses(NavLinkClasses);

		for (int32 NavLinkClassIdx = 0; NavLinkClassIdx < NavLinkClasses.Num(); ++NavLinkClassIdx)
		{
			if (NavLinkClasses[NavLinkClassIdx] != NULL)
			{
				StorePointLinks(LinkOwnerLocalToWorld, UNavLinkDefinition::GetLinksDefinition(NavLinkClasses[NavLinkClassIdx]));
				StoreSegmentLinks(LinkOwnerLocalToWorld, UNavLinkDefinition::GetSegmentLinksDefinition(NavLinkClasses[NavLinkClassIdx]));
			}
		}

		TArray<FNavigationLink> PointLinks;
		TArray<FNavigationSegmentLink> SegmentLinks;
		if (LinkOwnerHost->GetNavigationLinksArray(PointLinks, SegmentLinks))
		{
			StorePointLinks(LinkOwnerLocalToWorld, PointLinks);
			StoreSegmentLinks(LinkOwnerLocalToWorld, SegmentLinks);
		}
	}
}

void FNavLinkRenderingProxy::StorePointLinks(const FTransform& InLocalToWorld, const TArray<FNavigationLink>& LinksArray)
{
	const FNavigationLink* Link = LinksArray.GetData();
	for (int32 LinkIndex = 0; LinkIndex < LinksArray.Num(); ++LinkIndex, ++Link)
	{	
		FNavLinkDrawing LinkDrawing;
		LinkDrawing.Left = InLocalToWorld.TransformPosition(Link->Left);
		LinkDrawing.Right = InLocalToWorld.TransformPosition(Link->Right);
		LinkDrawing.Direction = Link->Direction;
		LinkDrawing.Color = UNavArea::GetColor(Link->GetAreaClass());
		LinkDrawing.SnapRadius = Link->SnapRadius;
		LinkDrawing.SnapHeight = Link->bUseSnapHeight ? Link->SnapHeight : -1.0f;
		LinkDrawing.SupportedAgentsBits = Link->SupportedAgents.PackedBits;
		OffMeshPointLinks.Add(LinkDrawing);
	}
}

void FNavLinkRenderingProxy::StoreSegmentLinks(const FTransform& InLocalToWorld, const TArray<FNavigationSegmentLink>& LinksArray)
{
	const FNavigationSegmentLink* Link = LinksArray.GetData();
	for (int32 LinkIndex = 0; LinkIndex < LinksArray.Num(); ++LinkIndex, ++Link)
	{	
		FNavLinkSegmentDrawing LinkDrawing;
		LinkDrawing.LeftStart = InLocalToWorld.TransformPosition(Link->LeftStart);
		LinkDrawing.LeftEnd = InLocalToWorld.TransformPosition(Link->LeftEnd);
		LinkDrawing.RightStart = InLocalToWorld.TransformPosition(Link->RightStart);
		LinkDrawing.RightEnd = InLocalToWorld.TransformPosition(Link->RightEnd);
		LinkDrawing.Direction = Link->Direction;
		LinkDrawing.Color = UNavArea::GetColor(Link->GetAreaClass());
		LinkDrawing.SnapRadius = Link->SnapRadius;
		LinkDrawing.SnapHeight = Link->bUseSnapHeight ? Link->SnapHeight : -1.0f;
		LinkDrawing.SupportedAgentsBits = Link->SupportedAgents.PackedBits;
		OffMeshSegmentLinks.Add(LinkDrawing);
	}
}

void FNavLinkRenderingProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const 
{
	if (LinkOwnerActor && LinkOwnerActor->GetWorld())
	{
		const UNavigationSystem* NavSys = LinkOwnerActor->GetWorld()->GetNavigationSystem();
		TArray<float> StepHeights;
		uint32 AgentMask = 0;
		if (NavSys != NULL)
		{
			StepHeights.Reserve(NavSys->NavDataSet.Num());
			for(int32 DataIndex = 0; DataIndex < NavSys->NavDataSet.Num(); ++DataIndex)
			{
				const ARecastNavMesh* NavMesh = Cast<const ARecastNavMesh>(NavSys->NavDataSet[DataIndex]);

				if (NavMesh != NULL)
				{
					AgentMask = NavMesh->IsDrawingEnabled() ? AgentMask | (1 << DataIndex) : AgentMask;
					if (NavMesh->AgentMaxStepHeight > 0 && NavMesh->IsDrawingEnabled())
					{
						StepHeights.Add(NavMesh->AgentMaxStepHeight);
					}
				}
			}
		}

		static const FColor RadiusColor(150, 160, 150, 48);
		FMaterialRenderProxy* const MeshColorInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy(GEngine->DebugMeshMaterial->GetRenderProxy(false), RadiusColor);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FNavLinkRenderingProxy::GetLinkMeshes(OffMeshPointLinks, OffMeshSegmentLinks, StepHeights, MeshColorInstance, ViewIndex, Collector, AgentMask);
			}
		}
	}
}

void FNavLinkRenderingProxy::GetLinkMeshes(const TArray<FNavLinkDrawing>& OffMeshPointLinks, const TArray<FNavLinkSegmentDrawing>& OffMeshSegmentLinks, TArray<float>& StepHeights, FMaterialRenderProxy* const MeshColorInstance, int32 ViewIndex, FMeshElementCollector& Collector, uint32 AgentMask)
{
	static const FColor LinkColor(0,0,166);
	static const float LinkArcThickness = 3.f;
	static const float LinkArcHeight = 0.4f;

	if (StepHeights.Num() == 0)
	{
		StepHeights.Add(FNavigationSystem::FallbackAgentHeight / 2);
	}

	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

	for (int32 LinkIndex = 0; LinkIndex < OffMeshPointLinks.Num(); ++LinkIndex)
	{
		const FNavLinkDrawing& Link = OffMeshPointLinks[LinkIndex];
		if ((Link.SupportedAgentsBits & AgentMask) == 0)
		{
			continue;
		}
		const uint32 Segments = FPlatformMath::Max<uint32>(LinkArcHeight*(Link.Right - Link.Left).Size() / 10, 8);
		DrawArc(PDI, Link.Left, Link.Right, LinkArcHeight, Segments, Link.Color, SDPG_World, 3.5f);
		const FVector VOffset(0,0,FVector::Dist(Link.Left, Link.Right)*1.333f);

		switch (Link.Direction)
		{
		case ENavLinkDirection::LeftToRight:
			DrawArrowHead(PDI, Link.Right, Link.Left+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::RightToLeft:
			DrawArrowHead(PDI, Link.Left, Link.Right+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::BothWays:
		default:
			DrawArrowHead(PDI, Link.Right, Link.Left+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.Left, Link.Right+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		}

		// draw snap-spheres on both ends
		if (Link.SnapHeight < 0)
		{
			for (int32 StepHeightIndex = 0; StepHeightIndex < StepHeights.Num(); ++StepHeightIndex)
			{
				GetCylinderMesh(Link.Right, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
				GetCylinderMesh(Link.Left, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			}
		}
		else
		{
			GetCylinderMesh(Link.Right, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			GetCylinderMesh(Link.Left, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
		}
	}

	static const float SegmentArcHeight = 0.25f;
	for (int32 LinkIndex = 0; LinkIndex < OffMeshSegmentLinks.Num(); ++LinkIndex)
	{
		const FNavLinkSegmentDrawing& Link = OffMeshSegmentLinks[LinkIndex];
		if ((Link.SupportedAgentsBits & AgentMask) == 0)
		{
			continue;
		}
		const uint32 SegmentsStart = FPlatformMath::Max<uint32>(SegmentArcHeight*(Link.RightStart - Link.LeftStart).Size() / 10, 8);
		const uint32 SegmentsEnd = FPlatformMath::Max<uint32>(SegmentArcHeight*(Link.RightEnd-Link.LeftEnd).Size()/10, 8);
		DrawArc(PDI, Link.LeftStart, Link.RightStart, SegmentArcHeight, SegmentsStart, Link.Color, SDPG_World, 3.5f);
		DrawArc(PDI, Link.LeftEnd, Link.RightEnd, SegmentArcHeight, SegmentsEnd, Link.Color, SDPG_World, 3.5f);
		const FVector VOffset(0,0,FVector::Dist(Link.LeftStart, Link.RightStart)*1.333f);

		switch (Link.Direction)
		{
		case ENavLinkDirection::LeftToRight:
			DrawArrowHead(PDI, Link.RightStart, Link.LeftStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.RightEnd, Link.LeftEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::RightToLeft:
			DrawArrowHead(PDI, Link.LeftStart, Link.RightStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftEnd, Link.RightEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::BothWays:
		default:
			DrawArrowHead(PDI, Link.RightStart, Link.LeftStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.RightEnd, Link.LeftEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftStart, Link.RightStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftEnd, Link.RightEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		}

		// draw snap-spheres on both ends
		if (Link.SnapHeight < 0)
		{
			for (int32 StepHeightIndex = 0; StepHeightIndex < StepHeights.Num(); ++StepHeightIndex)
			{
				GetCylinderMesh(Link.RightStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
				GetCylinderMesh(Link.RightEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
				GetCylinderMesh(Link.LeftStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
				GetCylinderMesh(Link.LeftEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			}
		}
		else
		{
			GetCylinderMesh(Link.RightStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			GetCylinderMesh(Link.RightEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			GetCylinderMesh(Link.LeftStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
			GetCylinderMesh(Link.LeftEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World, ViewIndex, Collector);
		}
	}
}

void FNavLinkRenderingProxy::DrawLinks(FPrimitiveDrawInterface* PDI, TArray<FNavLinkDrawing>& OffMeshPointLinks, TArray<FNavLinkSegmentDrawing>& OffMeshSegmentLinks, TArray<float>& StepHeights, FMaterialRenderProxy* const MeshColorInstance, uint32 AgentMask)
{
	static const FColor LinkColor(0,0,166);
	static const float LinkArcThickness = 3.f;
	static const float LinkArcHeight = 0.4f;

	if (StepHeights.Num() == 0)
	{
		StepHeights.Add(FNavigationSystem::FallbackAgentHeight / 2);
	}

	for (int32 LinkIndex = 0; LinkIndex < OffMeshPointLinks.Num(); ++LinkIndex)
	{
		const FNavLinkDrawing& Link = OffMeshPointLinks[LinkIndex];
		if ((Link.SupportedAgentsBits & AgentMask) == 0)
		{
			continue;
		}

		const uint32 Segments = FPlatformMath::Max<uint32>(LinkArcHeight*(Link.Right-Link.Left).Size()/10, 8);
		DrawArc(PDI, Link.Left, Link.Right, LinkArcHeight, Segments, Link.Color, SDPG_World, 3.5f);
		const FVector VOffset(0,0,FVector::Dist(Link.Left, Link.Right)*1.333f);

		switch (Link.Direction)
		{
		case ENavLinkDirection::LeftToRight:
			DrawArrowHead(PDI, Link.Right, Link.Left+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::RightToLeft:
			DrawArrowHead(PDI, Link.Left, Link.Right+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::BothWays:
		default:
			DrawArrowHead(PDI, Link.Right, Link.Left+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.Left, Link.Right+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		}

		// draw snap-spheres on both ends
		if (Link.SnapHeight < 0)
		{
			for (int32 StepHeightIndex = 0; StepHeightIndex < StepHeights.Num(); ++StepHeightIndex)
			{
				DrawCylinder(PDI, Link.Right, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
				DrawCylinder(PDI, Link.Left, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
			}
		}
		else
		{
			DrawCylinder(PDI, Link.Right, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
			DrawCylinder(PDI, Link.Left, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
		}
	}

	static const float SegmentArcHeight = 0.25f;
	for (int32 LinkIndex = 0; LinkIndex < OffMeshSegmentLinks.Num(); ++LinkIndex)
	{
		const FNavLinkSegmentDrawing& Link = OffMeshSegmentLinks[LinkIndex];
		if ((Link.SupportedAgentsBits & AgentMask) == 0)
		{
			continue;
		}

		const uint32 SegmentsStart = FPlatformMath::Max<uint32>(SegmentArcHeight*(Link.RightStart-Link.LeftStart).Size()/10, 8);
		const uint32 SegmentsEnd = FPlatformMath::Max<uint32>(SegmentArcHeight*(Link.RightEnd-Link.LeftEnd).Size()/10, 8);
		DrawArc(PDI, Link.LeftStart, Link.RightStart, SegmentArcHeight, SegmentsStart, Link.Color, SDPG_World, 3.5f);
		DrawArc(PDI, Link.LeftEnd, Link.RightEnd, SegmentArcHeight, SegmentsEnd, Link.Color, SDPG_World, 3.5f);
		const FVector VOffset(0,0,FVector::Dist(Link.LeftStart, Link.RightStart)*1.333f);

		switch (Link.Direction)
		{
		case ENavLinkDirection::LeftToRight:
			DrawArrowHead(PDI, Link.RightStart, Link.LeftStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.RightEnd, Link.LeftEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::RightToLeft:
			DrawArrowHead(PDI, Link.LeftStart, Link.RightStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftEnd, Link.RightEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		case ENavLinkDirection::BothWays:
		default:
			DrawArrowHead(PDI, Link.RightStart, Link.LeftStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.RightEnd, Link.LeftEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftStart, Link.RightStart+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			DrawArrowHead(PDI, Link.LeftEnd, Link.RightEnd+VOffset, 30.f, Link.Color, SDPG_World, 3.5f);
			break;
		}

		// draw snap-spheres on both ends
		if (Link.SnapHeight < 0)
		{
			for (int32 StepHeightIndex = 0; StepHeightIndex < StepHeights.Num(); ++StepHeightIndex)
			{
				DrawCylinder(PDI, Link.RightStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
				DrawCylinder(PDI, Link.RightEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
				DrawCylinder(PDI, Link.LeftStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
				DrawCylinder(PDI, Link.LeftEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, StepHeights[StepHeightIndex], 10, MeshColorInstance, SDPG_World);
			}
		}
		else
		{
			DrawCylinder(PDI, Link.RightStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
			DrawCylinder(PDI, Link.RightEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
			DrawCylinder(PDI, Link.LeftStart, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
			DrawCylinder(PDI, Link.LeftEnd, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Link.SnapRadius, Link.SnapHeight, 10, MeshColorInstance, SDPG_World);
		}
	}
}

FPrimitiveViewRelevance FNavLinkRenderingProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && IsSelected() && (View && View->Family && View->Family->EngineShowFlags.Navigation);
	Result.bDynamicRelevance = true;
	// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
	Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	return Result;
}

uint32 FNavLinkRenderingProxy::GetMemoryFootprint( void ) const 
{ 
	return( sizeof( *this ) + GetAllocatedSize() ); 
}

uint32 FNavLinkRenderingProxy::GetAllocatedSize( void ) const 
{ 
	return(FPrimitiveSceneProxy::GetAllocatedSize() + OffMeshPointLinks.GetAllocatedSize() + OffMeshSegmentLinks.GetAllocatedSize());
}
