// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" // for DLLEXPORT (KISMET_API)

#include "Kismet2/KismetEditorUtilities.h"

class UBlueprint;
class FCompilerResultsLog;

struct FBPCompileRequest
{
	explicit FBPCompileRequest(UBlueprint* InBPToCompile, EBlueprintCompileOptions InCompileOptions, FCompilerResultsLog* InClientResultsLog )
		: BPToCompile(InBPToCompile)
		, CompileOptions(InCompileOptions)
		, ClientResultsLog(InClientResultsLog)
	{
	}

	// BP that needs to be compiled:
	UBlueprint* BPToCompile;

	// Legacy options for blueprint compilation:
	EBlueprintCompileOptions CompileOptions;
	
	// Clients can give us a results log if they want to parse or display it themselves, otherwise
	// we will use a transient one:
	FCompilerResultsLog* ClientResultsLog;
};

struct KISMET_API FBlueprintCompilationManager
{
	static void Initialize();
	static void Shutdown();

	/**
	 * Compiles all blueprints that have been placed in the compilation queue. 
	 * ObjLoaded is a list of objects that need to be PostLoaded by the linker,
	 * when changing CDOs we will replace objects in this list. It is not a list
	 * of objects the compilation manager has loaded. The compilation manager
	 * will not load data while processing the compilation queue)
	 */
	static void FlushCompilationQueue(TArray<UObject*>* ObjLoaded = nullptr);
	
	/**
	 * Flushes the compilation queue and finishes reinstancing
	 */
	static void FlushCompilationQueueAndReinstance();

	/**
	 * Immediately compiles the blueprint, no expectation that related blueprints be subsequently compiled.
	 * It will be significantly more efficient to queue blueprints and then flush the compilation queue
	 * if there are several blueprints that require compilation (e.g. typical case on PIE):
	 */
	static void CompileSynchronously(const FBPCompileRequest& Request);
	
	/**
	 * Adds a newly loaded blueprint to the compilation queue
	 */
	static void NotifyBlueprintLoaded(UBlueprint* BPLoaded);

	/**
	 * Adds a blueprint to the compilation queue - useful for batch compilation
	 */
	static void QueueForCompilation(UBlueprint* BP);

	/** Returns true when UBlueprint::GeneratedClass members are up to date */
	static bool IsGeneratedClassLayoutReady();
	
	/** 
	 * Returns the Default Value associated with ForClass::Property, if ForClass is currently 
	 * being compiled this function can look at the old version of the CDO and read the default
	 * value from there
	 */
	static bool GetDefaultValue(const UClass* ForClass, const UProperty* Property, FString& OutDefaultValueAsString);
private:
	FBlueprintCompilationManager();
};

