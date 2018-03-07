// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ActorFactoryFlex.generated.h"

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryFlex : public UActorFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UActorFactory Interface
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	// End UActorFactory Interface
};



