// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ClothingSystemRuntimeModule.h"
#include "ClothingSimulationFactory.h"
#include "Features/IModularFeatures.h"
#include "Paths.h"
#include "HAL/PlatformProcess.h"
#include "ModuleManager.h"

#include "NvClothIncludes.h"
#include "NvClothSupport.h"


IMPLEMENT_MODULE(FClothingSystemRuntimeModule, ClothingSystemRuntime);

FClothingSystemRuntimeModule::FClothingSystemRuntimeModule()
#if WITH_NVCLOTH
	: ClothFactory(nullptr)
	, Quadifier(nullptr)
#if PLATFORM_WINDOWS
	, NvClothHandle(0)
#endif
#endif
{

}

void FClothingSystemRuntimeModule::StartupModule()
{
#if WITH_NVCLOTH
	DelayLoadNvCloth();

	NvClothSupport::InitializeNvClothingSystem();

	ClothFactory = NvClothCreateFactoryCPU();
	Quadifier = NvClothCreateMeshQuadifier();
#endif

	IModularFeatures::Get().RegisterModularFeature(IClothingSimulationFactoryClassProvider::FeatureName, this);
}

void FClothingSystemRuntimeModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(IClothingSimulationFactoryClassProvider::FeatureName, this);

#if WITH_NVCLOTH
	delete Quadifier;
	Quadifier = nullptr;

	NvClothDestroyFactory(ClothFactory);

	ShutdownNvClothLibs();
#endif
}

nv::cloth::Factory* FClothingSystemRuntimeModule::GetSoftwareFactory()
{
#if WITH_NVCLOTH
	ensureMsgf(ClothFactory, TEXT("Cloth SW factory has not been created yet!"));
	return ClothFactory;
#endif

	return nullptr;
}

nv::cloth::ClothMeshQuadifier* FClothingSystemRuntimeModule::GetMeshQuadifier()
{
#if WITH_NVCLOTH
	ensureMsgf(Quadifier, TEXT("Cloth quadifier has not been created yet!"));
	return Quadifier;
#endif

	return nullptr;
}

UClass* FClothingSystemRuntimeModule::GetDefaultSimulationFactoryClass()
{
#if WITH_NVCLOTH
	return UClothingSimulationFactoryNv::StaticClass();
#endif

	return nullptr;
}

#if WITH_NVCLOTH
void FClothingSystemRuntimeModule::DelayLoadNvCloth()
{
#if PLATFORM_WINDOWS
	DelayLoadNvCloth_Windows();
#elif PLATFORM_MAC
	DelayLoadNvCloth_Mac();
#endif
}

void FClothingSystemRuntimeModule::ShutdownNvClothLibs()
{
#if PLATFORM_WINDOWS
	ShutdownNvCloth_Windows();
#elif PLATFORM_MAC
	ShutdownNvCloth_Mac();
#endif
}

#if PLATFORM_WINDOWS
void FClothingSystemRuntimeModule::DelayLoadNvCloth_Windows()
{
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/");

#if _MSC_VER >= 1900
	FString VSDirectory(TEXT("VS2015/"));
#else
#error "Unrecognized Visual Studio version."
#endif

#if PLATFORM_64BITS
	FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win64/") + VSDirectory);
	FString ArchName(TEXT("_x64"));
#else
	FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win32/") + VSDirectory);
	FString ArchName(TEXT("_x86"));
#endif

#ifdef UE_NVCLOTH_SUFFIX
	FString LibSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_NVCLOTH_SUFFIX)) + ArchName + FString(TEXT(".dll")));
#else
	FString LibSuffix(ArchName + TEXT(".dll"));
#endif

	FString ModulePath = RootPhysXPath + TEXT("NvCloth") + LibSuffix;
	NvClothHandle = FPlatformProcess::GetDllHandle(*ModulePath);
	checkf(NvClothHandle, TEXT("Failed to load module: %s"), *ModulePath);
}

void FClothingSystemRuntimeModule::ShutdownNvCloth_Windows()
{
	if(NvClothHandle)
	{
		FPlatformProcess::FreeDllHandle(NvClothHandle);
	}
}

#endif // PLATFORM_WINDOWS

#if PLATFORM_MAC
void FClothingSystemRuntimeModule::DelayLoadNvCloth_Mac()
{
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/Mac/");

#ifdef UE_NVCLOTH_SUFFIX
	FString LibSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_NVCLOTH_SUFFIX)) + FString(TEXT(".dylib")));
#else
	FString LibSuffix(TEXT(".dylib"));
#endif

	FString ModulePath = PhysXBinariesRoot + TEXT("libNvCloth") + LibSuffix;
	NvClothHandle = FPlatformProcess::GetDllHandle(*ModulePath);
	checkf(NvClothHandle, TEXT("Failed to load module: %s"), *ModulePath);
}

void FClothingSystemRuntimeModule::ShutdownNvCloth_Mac()
{
	if(NvClothHandle)
	{
		FPlatformProcess::FreeDllHandle(NvClothHandle);
	}
}
#endif


#endif // WITH_NVCLOTH