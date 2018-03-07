// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ScriptCodeGeneratorBase.h"

/**
 * Generic script code generator. 
 * Generates wrapper functions for all classes that can be exported. Wrappers are not really usuable and serve educational purpose only.
 */
class FGenericScriptCodeGenerator : public FScriptCodeGeneratorBase
{
protected:

	/** All generated script header filenames */
	TArray<FString> AllScriptHeaders;
	/** Source header filenames for all exported classes */
	TArray<FString> AllSourceClassHeaders;
	
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
	/** Generates function parameter declaration */
	FString GenerateFunctionParamDeclaration(const FString& ClassNameCPP, UClass* Class, UFunction* Function, UProperty* Param);
	/** Generates code responsible for getting the object pointer from script context */
	FString GenerateObjectDeclarationFromContext(const FString& ClassNameCPP, UClass* Class);
	/** Handles the wrapped function's return value */
	FString GenerateReturnValueHandler(const FString& ClassNameCPP, UClass* Class, UFunction* Function, UProperty* ReturnValue);

	// FScriptCodeGeneratorBase
	virtual bool CanExportClass(UClass* Class) override;

public:

	FGenericScriptCodeGenerator(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase);

	// FScriptCodeGeneratorBase interface
	virtual void ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) override;
	virtual void FinishExport() override;
};
