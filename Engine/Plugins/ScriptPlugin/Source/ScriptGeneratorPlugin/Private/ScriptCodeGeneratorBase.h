// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* Base for script code generators.
* Contains utility functions that may be common to more specialized script glue generators.
*/
class FScriptCodeGeneratorBase
{
	/** List of temporary header files crated by SaveHeaderIfChanged */
	TArray<FString> TempHeaders;

protected:

	/** Path where generated script glue goes **/
	FString GeneratedCodePath;
	/** Local root path **/
	FString RootLocalPath;
	/** Build root path - may be different to RootBuildPath if we're building remotely. **/
	FString RootBuildPath;
	/** Base include directory */
	FString IncludeBase;
	/** Set of all exported class names */
	TSet<FName> ExportedClasses;

	/** Default ctor */
	FScriptCodeGeneratorBase(const FString& InRootLocalPath, const FString& InRootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase);

	/** Saves generated script glue heade to a temporary file if its contents is different from the eexisting one. */
	bool SaveHeaderIfChanged(const FString& HeaderPath, const FString& NewHeaderContents);
	/** Renames/replaces all existing script glue files with the temporary (new) ones */
	void RenameTempFiles();
	/** Re-bases the local path to build path */
	FString RebaseToBuildPath(const FString& FileName) const;
	/** Converts a UClass name to C++ class name (with U/A prefix) */
	FString GetClassNameCPP(UClass* Class);
	/** Gets C++ friendly property type name. */
	FString GetPropertyTypeCPP(UProperty* Property, uint32 PortFlags = 0);

	virtual FString GenerateFunctionDispatch(UFunction* Function);
	virtual FString InitializeFunctionDispatchParam(UFunction* Function, UProperty* Param, int32 ParamIndex);

	/** Generates a script glue header filename for the specified class */
	virtual FString GetScriptHeaderForClass(UClass* Class);
	/** Returns true if the specified class can be exported */
	virtual bool CanExportClass(UClass* Class);
	/** Returns true if the specified function can be exported */
	virtual bool CanExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function);
	/** Returns true if the specified property can be exported */
	virtual bool CanExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property);

public:

	virtual ~FScriptCodeGeneratorBase() {}

	// IScriptGeneratorPlugin interface
	virtual void ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) = 0;
	virtual void FinishExport() = 0;

};
