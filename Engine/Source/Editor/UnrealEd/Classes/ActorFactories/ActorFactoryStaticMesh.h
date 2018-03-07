// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryStaticMesh.generated.h"

class AActor;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryStaticMesh : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO ) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const override;

#if WITH_FLEX
	virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name) override;
#endif // WITH_FLEX

	//~ End UActorFactory Interface
};



