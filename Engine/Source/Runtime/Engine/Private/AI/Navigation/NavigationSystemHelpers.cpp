// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/NavigationSystemHelpers.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "AI/NavigationModifier.h"
#if WITH_RECAST
#include "AI/Navigation/RecastNavMeshGenerator.h"
#endif // WITH_RECAST
#include "VisualLogger/VisualLogger.h"
#include "AI/Navigation/NavCollision.h"
#include "PhysicsEngine/BodySetup.h"

namespace NavigationHelper
{
	FNavLinkOwnerData::FNavLinkOwnerData(const AActor& InActor)
	{
		Actor = &InActor;
		LinkToWorld = InActor.GetActorTransform();
	}

	FNavLinkOwnerData::FNavLinkOwnerData(const USceneComponent& InComponent)
	{
		Actor = InComponent.GetOwner();
		LinkToWorld = InComponent.GetComponentTransform();
	}

	void GatherCollision(UBodySetup* RigidBody, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& LocalToWorld)
	{
		if (RigidBody == NULL)
		{
			return;
		}
#if WITH_RECAST
		FRecastNavMeshGenerator::ExportRigidBodyGeometry(*RigidBody, OutVertexBuffer, OutIndexBuffer, LocalToWorld);
#endif // WITH_RECAST
	}

	void GatherCollision(UBodySetup* RigidBody, UNavCollision* NavCollision)
	{
		if (RigidBody == NULL || NavCollision == NULL)
		{
			return;
		}
#if WITH_RECAST
		FRecastNavMeshGenerator::ExportRigidBodyGeometry(*RigidBody
			, NavCollision->TriMeshCollision.VertexBuffer, NavCollision->TriMeshCollision.IndexBuffer
			, NavCollision->ConvexCollision.VertexBuffer, NavCollision->ConvexCollision.IndexBuffer
			, NavCollision->ConvexShapeIndices);
#endif // WITH_RECAST
	}

	FORCEINLINE_DEBUGGABLE float RawGeometryFall(const AActor* Querier, const FVector& FallStart, const float FallLimit)
	{
		float FallDownHeight = 0.f;

		UE_VLOG_SEGMENT(Querier, LogNavigation, Log, FallStart, FallStart + FVector(0, 0, -FallLimit)
				, FColor::Red, TEXT("TerrainTrace"));

		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RawGeometryFall), true, Querier);
		FHitResult Hit;
		const bool bHit = Querier->GetWorld()->LineTraceSingleByChannel(Hit, FallStart, FallStart+FVector(0,0,-FallLimit), ECC_WorldStatic, TraceParams);
		if( bHit )
		{
			UE_VLOG_LOCATION(Querier, LogNavigation, Log, Hit.Location, 15, FColor::Red, TEXT("%s")
				, Hit.Actor.IsValid() ? *Hit.Actor->GetName() : TEXT("NULL"));

			if (Cast<UStaticMeshComponent>(Hit.Component.Get()))
			{
				const FVector Loc = Hit.ImpactPoint;
				FallDownHeight = FallStart.Z - Loc.Z;
			}
		}

		return FallDownHeight;
	}

	void DefaultNavLinkProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationLink>& IN NavLinks)
	{
		FSimpleLinkNavModifier SimpleLink(NavLinks, OwnerData.LinkToWorld);

		// adjust links
		for (int32 LinkIndex = 0; LinkIndex < SimpleLink.Links.Num(); ++LinkIndex)
		{
			FNavigationLink& Link = SimpleLink.Links[LinkIndex];

			// this one needs adjusting
			if (Link.Direction == ENavLinkDirection::RightToLeft)
			{
				Swap(Link.Left, Link.Right);
			}

			if (Link.MaxFallDownLength > 0)
			{
				const FVector WorldRight = OwnerData.LinkToWorld.TransformPosition(Link.Right);
				const float FallDownHeight = RawGeometryFall(OwnerData.Actor, WorldRight, Link.MaxFallDownLength);

				if (FallDownHeight > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(OwnerData.Actor, LogNavigation, Log, WorldRight, WorldRight + FVector(0, 0, -FallDownHeight), FColor::Green, TEXT("FallDownHeight %d"), LinkIndex);

					Link.Right.Z -= FallDownHeight;
				}
			}

			if (Link.LeftProjectHeight > 0)
			{
				const FVector WorldLeft = OwnerData.LinkToWorld.TransformPosition(Link.Left);
				const float FallDownHeight = RawGeometryFall(OwnerData.Actor, WorldLeft, Link.LeftProjectHeight);

				if (FallDownHeight > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.LeftProjectHeight here
					UE_VLOG_SEGMENT(OwnerData.Actor, LogNavigation, Log, WorldLeft, WorldLeft + FVector(0, 0, -FallDownHeight), FColor::Green, TEXT("LeftProjectHeight %d"), LinkIndex);

					Link.Left.Z -= FallDownHeight;
				}
			}
		}

		CompositeModifier->Add(SimpleLink);
	}

	void DefaultNavLinkSegmentProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		FSimpleLinkNavModifier SimpleLink(NavLinks, OwnerData.LinkToWorld);

		// adjust links if needed
		for (int32 LinkIndex = 0; LinkIndex < SimpleLink.SegmentLinks.Num(); ++LinkIndex)
		{
			FNavigationSegmentLink& Link = SimpleLink.SegmentLinks[LinkIndex];

			// this one needs adjusting
			if (Link.Direction == ENavLinkDirection::RightToLeft)
			{
				Swap(Link.LeftStart, Link.RightStart);
				Swap(Link.LeftEnd, Link.RightEnd);
			}

			if (Link.MaxFallDownLength > 0)
			{
				const FVector WorldRightStart = OwnerData.LinkToWorld.TransformPosition(Link.RightStart);
				const FVector WorldRightEnd = OwnerData.LinkToWorld.TransformPosition(Link.RightEnd);

				const float FallDownHeightStart = RawGeometryFall(OwnerData.Actor, WorldRightStart, Link.MaxFallDownLength);
				const float FallDownHeightEnd = RawGeometryFall(OwnerData.Actor, WorldRightEnd, Link.MaxFallDownLength);

				if (FallDownHeightStart > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(OwnerData.Actor, LogNavigation, Log, WorldRightStart, WorldRightStart + FVector(0, 0, -FallDownHeightStart), FColor::Green, TEXT("FallDownHeightStart %d"), LinkIndex);

					Link.RightStart.Z -= FallDownHeightStart;
				}
				if (FallDownHeightEnd > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(OwnerData.Actor, LogNavigation, Log, WorldRightEnd, WorldRightEnd + FVector(0, 0, -FallDownHeightEnd), FColor::Green, TEXT("FallDownHeightEnd %d"), LinkIndex);

					Link.RightEnd.Z -= FallDownHeightEnd;
				}
			}
		}

		CompositeModifier->Add(SimpleLink);
	}

	FNavLinkProcessorDataDelegate NavLinkProcessor = FNavLinkProcessorDataDelegate::CreateStatic(DefaultNavLinkProcessorImpl);
	FNavLinkSegmentProcessorDataDelegate NavLinkSegmentProcessor = FNavLinkSegmentProcessorDataDelegate::CreateStatic(DefaultNavLinkSegmentProcessorImpl);

	void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks)
	{
		if (Actor)
		{
			ProcessNavLinkAndAppend(CompositeModifier, FNavLinkOwnerData(*Actor), NavLinks);
		}
	}

	void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationLink>& IN NavLinks)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_AdjustingNavLinks);

		if (NavLinks.Num())
		{
			check(NavLinkProcessor.IsBound());
			NavLinkProcessor.Execute(CompositeModifier, OwnerData, NavLinks);
		}
	}

	void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		if (Actor)
		{
			ProcessNavLinkSegmentAndAppend(CompositeModifier, FNavLinkOwnerData(*Actor), NavLinks);
		}
	}

	void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_AdjustingNavLinks);

		if (NavLinks.Num())
		{
			check(NavLinkSegmentProcessor.IsBound());
			NavLinkSegmentProcessor.Execute(CompositeModifier, OwnerData, NavLinks);
		}
	}

	void SetNavLinkProcessorDelegate(const FNavLinkProcessorDataDelegate& NewDelegate)
	{
		check(NewDelegate.IsBound());
		NavLinkProcessor = NewDelegate;
	}

	void SetNavLinkSegmentProcessorDelegate(const FNavLinkSegmentProcessorDataDelegate& NewDelegate)
	{
		check(NewDelegate.IsBound());
		NavLinkSegmentProcessor = NewDelegate;
	}

	bool IsBodyNavigationRelevant(const UBodySetup& BodySetup)
	{
		// has any colliding geometry
		return (BodySetup.AggGeom.GetElementCount() > 0 || BodySetup.TriMeshes.Num() > 0)
			// AND blocks any of Navigation-relevant 
			&& (BodySetup.DefaultInstance.GetResponseToChannel(ECC_Pawn) == ECR_Block || BodySetup.DefaultInstance.GetResponseToChannel(ECC_Vehicle) == ECR_Block)
			// AND has full colliding capabilities 
			&& BodySetup.DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics;
	}

	//////////////////////////////////////////////////////////////////////////
	// DEPRECATED FUNCTIONS

	void SetNavLinkProcessorDelegate(const FNavLinkProcessorDelegate& NewDelegate)
	{
		// don't set delegate, custom navlink processor will not be registered with deprecated function
	}

	void SetNavLinkSegmentProcessorDelegate(const FNavLinkSegmentProcessorDelegate& NewDelegate)
	{
		// don't set delegate, custom navlink processor will not be registered with deprecated function
	}

	void DefaultNavLinkProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks)
	{
		if (Actor)
		{
			DefaultNavLinkProcessorImpl(CompositeModifier, FNavLinkOwnerData(*Actor), NavLinks);
		}
	}

	void DefaultNavLinkSegmentProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		if (Actor)
		{
			DefaultNavLinkSegmentProcessorImpl(CompositeModifier, FNavLinkOwnerData(*Actor), NavLinks);
		}
	}
}
