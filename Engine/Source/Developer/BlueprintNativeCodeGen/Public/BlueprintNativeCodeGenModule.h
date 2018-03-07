// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Engine/Blueprint.h"

class UBlueprint;
enum class ESavePackageResult;
class ITargetPlatform;

struct FPlatformNativizationDetails
{
	FName PlatformName;
	FCompilerNativizationOptions CompilerNativizationOptions;
};

struct FNativeCodeGenInitData
{
	// This is an array platforms. These are determined by the cooker:
	TArray< FPlatformNativizationDetails > CodegenTargets;

	// Optional Manifest ManifestIdentifier, used for child cook processes that need a unique manifest name.
	// The identifier is used to make a unique name for each platform that is converted.
	int32 ManifestIdentifier;
};

/** 
 * 
 */
class IBlueprintNativeCodeGenModule : public IModuleInterface
{
public:
	FORCEINLINE static void InitializeModule(const FNativeCodeGenInitData& InitData);

	/**
	* Utility function to reconvert all assets listed in a manifest, used to make fixes to
	* the code generator itself and quickly test them with an already converted project.
	*
	* Not for use with any kind of incremental cooking.
	*/
	FORCEINLINE static void InitializeModuleForRerunDebugOnly(const TArray<FPlatformNativizationDetails>& CodegenTargets);

	/**
	 * Wrapper function that retrieves the interface to this module from the 
	 * module-manager (so we can keep dependent code free of hardcoded strings,
	 * used to lookup this module by name).
	 * 
	 * @return A reference to this module interface (a singleton).
	 */
	static IBlueprintNativeCodeGenModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	}

	static bool IsNativeCodeGenModuleLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(GetModuleName());
	}
	
	/**
	 * Creates a centralized point where the name of this module is supplied 
	 * from (so we can avoid littering code with hardcoded strings that 
	 * all reference this module - in case we want to rename it).
	 * 
	 * @return The name of the module that this file is part of.
	 */
	static FName GetModuleName()
	{
		return TEXT("BlueprintNativeCodeGen");
	}

	virtual void Convert(UPackage* Package, ESavePackageResult ReplacementType, const FName PlatformName) = 0;
	virtual void SaveManifest() = 0;
	virtual void MergeManifest(int32 ManifestIdentifier) = 0;
	virtual void FinalizeManifest() = 0;
	virtual void GenerateStubs() = 0;
	virtual void GenerateFullyConvertedClasses() = 0;
	virtual void MarkUnconvertedBlueprintAsNecessary(TSoftObjectPtr<UBlueprint> BPPtr, const FCompilerNativizationOptions& NativizationOptions) = 0;
	virtual const TMultiMap<FName, TSoftClassPtr<UObject>>& GetFunctionsBoundToADelegate() = 0;

	virtual void FillPlatformNativizationDetails(const ITargetPlatform* Platform, FPlatformNativizationDetails& OutDetails) = 0;
protected:
	virtual void Initialize(const FNativeCodeGenInitData& InitData) = 0;
	virtual void InitializeForRerunDebugOnly(const TArray<FPlatformNativizationDetails>& CodegenTargets) = 0;
};

void IBlueprintNativeCodeGenModule::InitializeModule(const FNativeCodeGenInitData& InitData)
{
	IBlueprintNativeCodeGenModule& Module = FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	Module.Initialize(InitData);
}

void IBlueprintNativeCodeGenModule::InitializeModuleForRerunDebugOnly(const TArray<FPlatformNativizationDetails>& CodegenTargets)
{
	IBlueprintNativeCodeGenModule& Module = FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	Module.InitializeForRerunDebugOnly(CodegenTargets);
}

