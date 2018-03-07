// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavLinkComponent.h"
#include "AI/NavigationOctree.h"
#include "AI/NavLinkRenderingProxy.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "Engine/CollisionProfile.h"

UNavLinkComponent::UNavLinkComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Stationary;
	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;

	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::EvenIfNotCollidable;
	bCanEverAffectNavigation = true;
	bNavigationRelevant = true;

	FNavigationLink DefLink;
	DefLink.SetAreaClass(UNavArea_Default::StaticClass());

	Links.Add(DefLink);
}

FBoxSphereBounds UNavLinkComponent::CalcBounds(const FTransform &LocalToWorld) const
{
	FBox LocalBounds(ForceInit);
	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		LocalBounds += Links[Idx].Left;
		LocalBounds += Links[Idx].Right;
	}

	const FBox WorldBounds = LocalBounds.TransformBy(LocalToWorld);
	return FBoxSphereBounds(WorldBounds);
}

void UNavLinkComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	NavigationHelper::ProcessNavLinkAndAppend(&Data.Modifiers, NavigationHelper::FNavLinkOwnerData(*this), Links);
}

bool UNavLinkComponent::IsNavigationRelevant() const
{
	return Links.Num() > 0;
}

bool UNavLinkComponent::GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const
{
	OutLink.Append(Links);
	return OutLink.Num() > 0;
}

FPrimitiveSceneProxy* UNavLinkComponent::CreateSceneProxy()
{
	return new FNavLinkRenderingProxy(this);
}

#if WITH_EDITOR

void UNavLinkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNavLinkComponent, Links))
	{
		for (FNavigationLink& Link : Links)
		{
			Link.InitializeAreaClass(/*bForceRefresh=*/true);
		}
	}
}

void UNavLinkComponent::PostEditUndo()
{
	Super::PostEditUndo();

	for (FNavigationLink& Link : Links)
	{
		Link.InitializeAreaClass(/*bForceRefresh=*/true);
	}
}

void UNavLinkComponent::PostEditImport()
{
	Super::PostEditImport();

	for (FNavigationLink& Link : Links)
	{
		Link.InitializeAreaClass(/*bForceRefresh=*/true);
	}
}

#endif // WITH_EDITOR