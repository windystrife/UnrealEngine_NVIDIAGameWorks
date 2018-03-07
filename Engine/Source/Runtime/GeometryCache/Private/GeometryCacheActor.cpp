// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheActor.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"

AGeometryCacheActor::AGeometryCacheActor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GeometryCacheComponent = CreateDefaultSubobject<UGeometryCacheComponent>(TEXT("GeometryCacheComponent"));
	RootComponent = GeometryCacheComponent;
}

GEOMETRYCACHE_API UGeometryCacheComponent* AGeometryCacheActor::GetGeometryCacheComponent() const
{
	return GeometryCacheComponent;
}

#if WITH_EDITOR
bool AGeometryCacheActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (GeometryCacheComponent && GeometryCacheComponent->GeometryCache)
	{
		Objects.Add(GeometryCacheComponent->GeometryCache);
	}

	return true;
}
#endif
