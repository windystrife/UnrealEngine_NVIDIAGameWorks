// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentAssetBroker.h"

/** AssetBroker class for GeometryCache assets*/
class FGeometryCacheAssetBroker : public IComponentAssetBroker
{
public:
	// Begin IComponentAssetBroker Implementation
	UClass* GetSupportedAssetClass() override;
	virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override;
	virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override;
	// End IComponentAssetBroker Implementation
};
