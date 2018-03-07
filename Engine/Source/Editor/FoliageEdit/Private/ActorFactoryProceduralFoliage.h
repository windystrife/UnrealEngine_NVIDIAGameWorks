// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "ActorFactoryProceduralFoliage.generated.h"

class AActor;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryProceduralFoliage : public UActorFactoryBoxVolume
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual bool PreSpawnActor(UObject* Asset, FTransform& InOutLocation);
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO ) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	//~ End UActorFactory Interface
};
