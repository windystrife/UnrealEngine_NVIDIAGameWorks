// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ScriptCodeGeneratorBase.h"

/**
 * Lua glue generator. 
 */
class FLuaScriptCodeGenerator : public FScriptCodeGeneratorBase
{
protected:

	struct FPropertyAccessor
	{
		FString FunctionName;
		FString AccessorName;
	};

	/** All generated script header filenames */
	TArray<FString> AllScriptHeaders;
	/** Source header filenames for all exported classes */
	TArray<FString> AllSourceClassHeaders;
	/** All exported classes */
	TArray<UClass*> LuaExportedClasses;
	/** Functions exported for a class */
	TMap<UClass*, TArray<FName> > ClassExportedFunctions;
	/** Proprties exported for a class */
	TMap<UClass*, TArray<FPropertyAccessor> > ClassExportedProperties;

	/** Creats a 'glue' file that merges all generated script files */
	void GlueAllGeneratedFiles();

	/** Exports a wrapper function */
	FString ExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function);
	/** Exports a wrapper functions for properties */
	FString ExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property, int32 PropertyIndex);
	/** Exports additional class glue code (like ctor/dtot) */
	FString ExportAdditionalClassGlue(const FString& ClassNameCPP, UClass* Class);
	/** Generates wrapper function declaration */
	FString GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, UFunction* Function);
	/** Generates wrapper function declaration */
	FString GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, const FString& FunctionName);
	/** Generates code responsible for getting the object pointer from script context */
	FString GenerateObjectDeclarationFromContext(const FString& ClassNameCPP, UClass* Class);
	/** Handles the wrapped function's return value */
	FString GenerateReturnValueHandler(const FString& ClassNameCPP, UClass* Class, UFunction* Function, UProperty* ReturnValue, const FString& ReturnValueName);
	/** Check if a property type is supported */
	bool IsPropertyTypeSupported(UProperty* Property) const;

	// FScriptCodeGeneratorBase interface
	virtual bool CanExportClass(UClass* Class) override;
	virtual bool CanExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function) override;
	virtual bool CanExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property) override;
	virtual FString InitializeFunctionDispatchParam(UFunction* Function, UProperty* Param, int32 ParamIndex) override;

public:

	FLuaScriptCodeGenerator(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase);

	// FScriptCodeGeneratorBase interface
	virtual void ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) override;
	virtual void FinishExport() override;
};
