// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ClothingSimulationFactoryInterface.h"

namespace nv
{
	namespace cloth
	{
		class Factory;
		class ClothMeshQuadifier;
	}
}

class FClothingSystemRuntimeModule : public IModuleInterface, public IClothingSimulationFactoryClassProvider
{

public:

	FClothingSystemRuntimeModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	nv::cloth::Factory* GetSoftwareFactory();
	nv::cloth::ClothMeshQuadifier* GetMeshQuadifier();

	// IClothingSimulationFactoryClassProvider Interface
	virtual UClass* GetDefaultSimulationFactoryClass() override;
	//////////////////////////////////////////////////////////////////////////

private:

#if WITH_NVCLOTH
	nv::cloth::Factory* ClothFactory;
	nv::cloth::ClothMeshQuadifier* Quadifier;

	void DelayLoadNvCloth();
	void ShutdownNvClothLibs();

#if PLATFORM_WINDOWS || PLATFORM_MAC
	void* NvClothHandle;
#endif

#if PLATFORM_WINDOWS
	void DelayLoadNvCloth_Windows();
	void ShutdownNvCloth_Windows();
#endif

#if PLATFORM_MAC
	void DelayLoadNvCloth_Mac();
	void ShutdownNvCloth_Mac();
#endif

#endif
};
