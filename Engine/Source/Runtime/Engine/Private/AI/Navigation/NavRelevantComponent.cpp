// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavRelevantComponent.h"
#include "AI/Navigation/NavigationSystem.h"

UNavRelevantComponent::UNavRelevantComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bCanEverAffectNavigation = true;
	bNavigationRelevant = true;
	bAttachToOwnersRoot = true;
	bBoundsInitialized = false;
}

void UNavRelevantComponent::OnRegister()
{
	Super::OnRegister();

	if (bAttachToOwnersRoot)
	{
		bool bUpdateCachedParent = true;
#if WITH_EDITOR
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		if (NavSys && NavSys->IsNavigationRegisterLocked())
		{
			bUpdateCachedParent = false;
		}
#endif

		AActor* OwnerActor = GetOwner();
		if (OwnerActor && bUpdateCachedParent)
		{
			// attach to root component if it's relevant for navigation
			UActorComponent* ActorComp = OwnerActor->GetRootComponent();			
			INavRelevantInterface* NavInterface = ActorComp ? Cast<INavRelevantInterface>(ActorComp) : nullptr;
			if (NavInterface && NavInterface->IsNavigationRelevant() &&
				OwnerActor->IsComponentRelevantForNavigation(ActorComp))
			{
				CachedNavParent = ActorComp;
			}

			// otherwise try actor itself under the same condition
			if (CachedNavParent == nullptr)
			{
				NavInterface = Cast<INavRelevantInterface>(OwnerActor);
				if (NavInterface && NavInterface->IsNavigationRelevant())
				{
					CachedNavParent = OwnerActor;
				}
			}
		}
	}

	UNavigationSystem::OnComponentRegistered(this);
}

void UNavRelevantComponent::OnUnregister()
{
	Super::OnUnregister();

	UNavigationSystem::OnComponentUnregistered(this);
}

FBox UNavRelevantComponent::GetNavigationBounds() const
{
	if (!bBoundsInitialized)
	{
		CalcAndCacheBounds();
		bBoundsInitialized = true;
	}

	return Bounds;
}

bool UNavRelevantComponent::IsNavigationRelevant() const
{
	return bNavigationRelevant;
}

void UNavRelevantComponent::UpdateNavigationBounds()
{
	CalcAndCacheBounds();
	bBoundsInitialized = true;
}

UObject* UNavRelevantComponent::GetNavigationParent() const
{
	return CachedNavParent;
}

void UNavRelevantComponent::CalcAndCacheBounds() const
{
	const AActor* OwnerActor = GetOwner();
	const FVector MyLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	Bounds = FBox::BuildAABB(MyLocation, FVector(100.0f, 100.0f, 100.0f));
}

void UNavRelevantComponent::ForceNavigationRelevancy(bool bForce)
{
	bAttachToOwnersRoot = !bForce;
	if (bForce)
	{
		bNavigationRelevant = true;
	}

	RefreshNavigationModifiers();
}

void UNavRelevantComponent::SetNavigationRelevancy(bool bRelevant)
{
	if (bNavigationRelevant != bRelevant)
	{
		bNavigationRelevant = bRelevant;
		RefreshNavigationModifiers();
	}
}

void UNavRelevantComponent::RefreshNavigationModifiers()
{
	UNavigationSystem::UpdateComponentInNavOctree(*this);
}
