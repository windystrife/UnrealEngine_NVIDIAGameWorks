// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulation.h"
#include "ClothingSimulationFactoryInterface.h"

#include "ClothingSimulationFactory.generated.h"

UCLASS()
class CLOTHINGSYSTEMRUNTIME_API UClothingSimulationFactoryNv final : public UClothingSimulationFactory
{
	GENERATED_BODY()
public:

	virtual IClothingSimulation* CreateSimulation() override;
	virtual void DestroySimulation(IClothingSimulation* InSimulation) override;
	virtual bool SupportsAsset(UClothingAssetBase* InAsset) override;
};