// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "KismetCompiledFunctionContext.h"
#include "Engine/Blueprint.h" // for FCompilerNativizationOptions

class UUserDefinedEnum;
class UUserDefinedStruct;

class IBlueprintCompilerCppBackend
{
public:
	virtual FString GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly, const FCompilerNativizationOptions& NativizationOptions, FString& OutCppBody) = 0;
	virtual void GenerateCodeFromEnum(UUserDefinedEnum* SourceEnum, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) = 0;
	virtual void GenerateCodeFromStruct(UUserDefinedStruct* SourceStruct, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) = 0;

	// Generate a wrapper class, that helps accessing non-native properties and calling non-native functions
	virtual FString GenerateWrapperForClass(UClass* SourceClass, const FCompilerNativizationOptions& NativizationOptions) = 0;

	virtual ~IBlueprintCompilerCppBackend() {}
};

/**
 * BlueprintCompilerCppBackend module interface
 */
class IBlueprintCompilerCppBackendModuleInterface : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IBlueprintCompilerCppBackendModuleInterface& Get()
	{
		return FModuleManager::LoadModuleChecked< IBlueprintCompilerCppBackendModuleInterface >("BlueprintCompilerCppBackend");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("BlueprintCompilerCppBackend");
	}

	virtual IBlueprintCompilerCppBackend* Create() = 0;
};
