// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DestructibleActor.cpp: ADestructibleActor methods.
=============================================================================*/


#include "DestructibleActor.h"
#include "DestructibleComponent.h"
#include "Engine/SkeletalMesh.h"

ADestructibleActor::ADestructibleActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DestructibleComponent = CreateDefaultSubobject<UDestructibleComponent>(TEXT("DestructibleComponent0"));
	RootComponent = DestructibleComponent;
}

#if WITH_EDITOR
bool ADestructibleActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);

	if (DestructibleComponent && DestructibleComponent->SkeletalMesh)
	{
		Objects.Add(DestructibleComponent->SkeletalMesh);
	}
	return true;
}

void ADestructibleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ADestructibleActor, bAffectNavigation))
	{
		DestructibleComponent->SetCanEverAffectNavigation(bAffectNavigation);
	}
}
#endif // WITH_EDITOR

void ADestructibleActor::PostLoad()
{
	Super::PostLoad();
	
	if (DestructibleComponent)
	{
		DestructibleComponent->SetCanEverAffectNavigation(bAffectNavigation);
	}
}

