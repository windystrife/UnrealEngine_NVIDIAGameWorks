// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorFactoryGeometryCache.h"
#include "AssetData.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheActor.h"

UActorFactoryGeometryCache::UActorFactoryGeometryCache(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = FText::FromString( "Geometry Cache" );
	NewActorClass = AGeometryCacheActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryGeometryCache::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UGeometryCache::StaticClass()))
	{
		OutErrorMsg = FText::FromString("A valid GeometryCache must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryGeometryCache::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UGeometryCache* GeometryCache = CastChecked<UGeometryCache>(Asset);

	// Change properties
	AGeometryCacheActor* GeometryCacheActor = CastChecked<AGeometryCacheActor>(NewActor);
	UGeometryCacheComponent* GeometryCacheComponent = GeometryCacheActor->GetGeometryCacheComponent();
	check(GeometryCacheComponent);

	GeometryCacheComponent->UnregisterComponent();

	// Set GeometryCache (data) instance
	GeometryCacheComponent->GeometryCache = GeometryCache;

	// Init Component
	GeometryCacheComponent->RegisterComponent();
}

void UActorFactoryGeometryCache::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		// Set GeometryCache (data) instance
		UGeometryCache* GeometryCache = CastChecked<UGeometryCache>(Asset);
		AGeometryCacheActor* GeometryCacheActor = CastChecked<AGeometryCacheActor>(CDO);
		UGeometryCacheComponent* GeometryCacheComponent = GeometryCacheActor->GetGeometryCacheComponent();

		GeometryCacheComponent->GeometryCache = GeometryCache;
	}
}
