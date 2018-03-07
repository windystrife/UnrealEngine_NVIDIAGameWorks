// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "TileMapActorFactory.generated.h"

class AActor;
struct FAssetData;

UCLASS()
class UTileMapActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	// UActorFactory interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	// End of UActorFactory interface
};
