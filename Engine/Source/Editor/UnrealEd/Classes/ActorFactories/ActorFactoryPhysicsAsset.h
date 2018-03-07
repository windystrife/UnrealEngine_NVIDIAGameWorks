// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryPhysicsAsset.generated.h"

class AActor;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class UActorFactoryPhysicsAsset : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual bool PreSpawnActor( UObject* Asset, FTransform& InOutLocation ) override;
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	//~ End UActorFactory Interface
};



