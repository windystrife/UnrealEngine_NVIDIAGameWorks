// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryEmptyActor.generated.h"

class AActor;
struct FAssetData;
class ULevel;

UCLASS(MinimalAPI,config=Editor)
class UActorFactoryEmptyActor : public UActorFactory
{
	GENERATED_UCLASS_BODY()

public:
	/** If true a sprite will be added to visualize the actor in the world */
	UPROPERTY()
	bool bVisualizeActor;
public:
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;

protected:
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name ) override;

};
