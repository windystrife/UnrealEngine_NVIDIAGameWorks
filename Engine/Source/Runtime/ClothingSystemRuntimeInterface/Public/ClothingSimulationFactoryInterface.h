// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationInterface.h"

#include "Object.h"
#include "Features/IModularFeature.h"

#include "ClothingSimulationFactoryInterface.generated.h"

class UClothingAssetBase;

// An interface for a class that will provide default simulation factory classes
// Used by modules wanting to override clothing simulation to provide their own implementation
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API IClothingSimulationFactoryClassProvider : public IModularFeature
{
public:

	// The feature name to register against for providers
	static const FName FeatureName;

	// Called by the engine to get the default clothing simulation factory to use
	// for skeletal mesh components (see USkeletalMeshComponent constructor).
	// Returns Factory class for simulations or nullptr to disable clothing simulation
	virtual UClass* GetDefaultSimulationFactoryClass() = 0;
};

// Any clothing simulation factory should derive from this interface object to interact with the engine
UCLASS(Abstract)
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API UClothingSimulationFactory : public UObject
{
	GENERATED_BODY()

public:

	// Create a simulation object for a skeletal mesh to use (see IClothingSimulation)
	virtual IClothingSimulation* CreateSimulation()
	PURE_VIRTUAL(UClothingSimulationFactory::CreateSimulation, return nullptr;);

	// Destroy a simulation object, guaranteed to be a pointer returned from CreateSimulation for this factory
	virtual void DestroySimulation(IClothingSimulation* InSimulation)
	PURE_VIRTUAL(UClothingSimulationFactory::DestroySimulation, );

	// Given an asset, decide whether this factory can create a simulation to use the data inside
	// (return false if data is invalid or missing in the case of custom data)
	virtual bool SupportsAsset(UClothingAssetBase* InAsset)
	PURE_VIRTUAL(UClothingSimulationFactory::SupportsAsset, return false;);
};